// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/time.h>
#include <linux/string.h>
#include "mtk_disp_bdg.h"
#include "mtk_reg_disp_bdg.h"
#include "mtk_log.h"
#include "mt6382.h"
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/gpio/consumer.h>
#include "clkbuf_v1/mt6853/mtk_clkbuf_hw.h"
#include <linux/soc/mediatek/mtk-cmdq.h>
#include "cmdq-bdg.h"
#include <linux/kthread.h>
#include <linux/sched.h>
#include <uapi/linux/sched/types.h>

/***** NFC SRCLKENAI0 Interrupt Handler +++ *****/
#include <linux/gpio.h>
#include <linux/of_gpio.h>
/***** NFC SRCLKENAI0 Interrupt Handler --- *****/

#define SPI_EN
struct BDG_SYSREG_CTRL_REGS *SYS_REG;		/* 0x00000000 */
struct BDG_TOPCKGEN_REGS *TOPCKGEN;		/* 0x00003000 */
struct BDG_APMIXEDSYS_REGS *APMIXEDSYS;		/* 0x00004000 */
struct BDG_GPIO_REGS *GPIO;			/* 0x00007000 */
struct BDG_EFUSE_REGS *EFUSE;			/* 0x00009000 */
struct BDG_MIPIDSI2_REGS *DSI2_REG;		/* 0x0000d000 */
struct BDG_GCE_REGS *GCE_REG;			/* 0x00010000 */
struct BDG_OCLA_REGS *OCLA_REG;			/* 0x00014000 */
struct BDG_DISP_DSC_REGS *DSC_REG;		/* 0x00020000 */
struct BDG_TX_REGS *TX_REG[HW_NUM];		/* 0x00021000 */
struct DSI_TX_CMDQ_REGS *TX_CMDQ_REG[HW_NUM];	/* 0x00021d00 */
struct BDG_MIPI_TX_REGS *MIPI_TX_REG;		/* 0x00022000 */
struct BDG_DISPSYS_CONFIG_REGS *DISPSYS_REG;	/* 0x00023000 */
struct BDG_RDMA0_REGS *RDMA_REG;		/* 0x00024000 */
struct BDG_MUTEX_REGS *MUTEX_REG;		/* 0x00025000 */
struct DSI_TX_PHY_TIMCON0_REG timcon0;
struct DSI_TX_PHY_TIMCON1_REG timcon1;
struct DSI_TX_PHY_TIMCON2_REG timcon2;
struct DSI_TX_PHY_TIMCON3_REG timcon3;
unsigned int bg_tx_data_phy_cycle = 0, tx_data_rate = 0, ap_tx_data_rate = 0;
unsigned int hsa_byte = 0, hbp_byte = 0, hfp_byte = 0, bllp_byte = 0, bg_tx_line_cycle = 0;
unsigned int dsc_en;
unsigned int mt6382_init;
unsigned int need_6382_init;
unsigned int bdg_tx_mode;
static int bdg_eint_irq;
static bool irq_already_requested;

/***** NFC SRCLKENAI0 Interrupt Handler +++ *****/
static int nfc_eint_irq;
static int mt6382_nfc_srclk;
static bool nfc_irq_already_requested;
static bool nfc_clk_already_enabled;
static int mt6382_nfc_gpio_value;
/***** NFC SRCLKENAI0 Interrupt Handler --- *****/

#define T_DCO		5  // nominal: 200MHz
int hsrx_clk_div;
int hs_thssettle, fjump_deskew_reg, eye_open_deskew_reg;
int cdr_coarse_trgt_reg, en_dly_deass_thresh_reg;
int max_phase, dll_fbk, coarse_bank, sel_fast;
int post_rcvd_rst_val, post_det_dly_thresh_val;
unsigned int post_rcvd_rst_reg, post_det_dly_thresh_reg;
unsigned int ddl_cntr_ref_reg;

#define REGFLAG_DELAY		0xFFFC
#define REGFLAG_UDELAY		0xFFFB
#define REGFLAG_END_OF_TABLE	0xFFFD
#define REGFLAG_RESET_LOW	0xFFFE
#define REGFLAG_RESET_HIGH	0xFFFF

struct lcm_setting_table {
	unsigned int cmd;
	unsigned char count;
	unsigned char para_list[256];
};

#define MM_CLK			405 //fpga=26
#define NS_TO_CYCLE_BDG(n, c)	((n) / (c) + (((n) % (c)) ? 1 : 0))

#define DSI_MODULE_to_ID(x)	(x == DISP_BDG_DSI0 ? 0 : 1)
#define DSI_MODULE_BEGIN(x)	(x == DISP_BDG_DSIDUAL ? 0 : DSI_MODULE_to_ID(x))
#define DSI_MODULE_END(x)	(x == DISP_BDG_DSIDUAL ? 1 : DSI_MODULE_to_ID(x))

#define DSI_SET_VAL(cmdq, addr, val) \
			((*(unsigned int *)(addr)) = (unsigned int)val)
#define DSI_GET_VAL(addr) (*(unsigned int *)(addr))

//#define SW_EARLY_PORTING
unsigned int mtk_spi_read(u32 addr)
{
	unsigned int value = 0;

#ifdef SW_EARLY_PORTING
#else
	spislv_read(addr, &value, 4);
#endif
//	DDPMSG("%s, addr=0x%08x, value=0x%08x\n", __func__, addr, value);
	return value;
}

int mtk_spi_write(u32 addr, unsigned int regval)
{
	unsigned int value = regval;
	int ret = 0;

//	DDPMSG("mt6382, %s, addr=0x%08x, value=0x%x\n", __func__, addr, value);
#ifdef SW_EARLY_PORTING
#else
	ret = spislv_write(addr, &value, 4);
#endif
	return ret;
}

int mtk_spi_mask_write(u32 addr, u32 msk, u32 value)
{
	unsigned int i = 0;
	unsigned int temp;

	for (i = 0; i < 32; i++) {
		temp = 1 << i;
		if ((msk & temp) == temp)
			break;
	}
	value = value << i;
//	DDPMSG("mt6382, %s, i=%02d, temp=0x%08x, addr=0x%08x, msk=0x%08x, value=0x%x\n",
//		__func__, i, temp, addr, msk, value);
#ifdef SW_EARLY_PORTING
	return 0;
#else
	return spislv_write_register_mask(addr, value, msk);
#endif
}

int mtk_spi_mask_field_write(u32 addr, u32 msk, u32 value)
{
	unsigned int i = 0;
	unsigned int temp;

	for (i = 0; i < 32; i++) {
		temp = 1 << i;
		if ((msk & temp) == temp)
			break;
	}
	value = value << i;
//	DDPMSG("mt6382, %s, i=%02d, temp=0x%08x, addr=0x%08x, msk=0x%08x, value=0x%x\n",
//		__func__, i, temp, addr, msk, value);

#ifdef SW_EARLY_PORTING
	return 0;
#else
	return spislv_write_register_mask(addr, value, msk);
#endif
}

#ifdef SPI_EN
#define BDG_OUTREGBIT(cmdq, TYPE, REG, bit, value) \
do { \
	TYPE r; \
	*(unsigned int *)(&r) = (0x00000000); \
	r.bit = ~(r.bit); \
	mtk_spi_mask_write((unsigned long)(&REG), AS_UINT32(&r), value); \
} while (0)

#define BDG_OUTREG32(cmdq, addr, val) \
do { \
	DDPMSG("%s\n", __func__); \
	mtk_spi_write((unsigned long)(&addr), val); \
} while (0)

#define BDG_MASKREG32(cmdq, addr, mask, val) \
do { \
	DDPMSG("%s\n", __func__); \
	mtk_spi_mask_write((unsigned long)(addr), mask, val); \
} while (0)
#else
#define BDG_OUTREGBIT(spi_en, cmdq, TYPE, REG, bit, value) \
do { \
	TYPE r; \
	TYPE v; \
	if (spi_en) { \
		*(unsigned int *)(&r) = (0x00000000); \
		r.bit = ~(r.bit); \
		mtk_spi_mask_write((unsigned long)(&REG), AS_UINT32(&r), value); \
	} \
	else { \
		if (cmdq) { \
			*(unsigned int *)(&r) = (0x00000000); \
			r.bit = ~(r.bit);  \
			*(unsigned int *)(&v) = (0x00000000); \
			v.bit = value; \
			DISP_REG_MASK(cmdq, &REG, AS_UINT32(&v), AS_UINT32(&r)); \
		} else { \
			DSI_SET_VAL(NULL, &r, INREG32(&REG)); \
			r.bit = (value); \
			DISP_REG_SET(cmdq, &REG, DSI_GET_VAL(&r)); \
		} \
	} \
} while (0)

#define BDG_OUTREG32(spi_en, cmdq, addr, val) \
do { \
	if (spi_en) { \
		mtk_spi_write((unsigned long)(&addr), val); \
	} \
	else { \
		DISP_REG_SET(cmdq, addr, val); \
	} \
} while (0)

#define BDG_MASKREG32(spi_en, cmdq, addr, mask, val) \
do { \
	if (spi_en) { \
		mtk_spi_mask_write((unsigned long)(addr), mask, val); \
	} \
	else { \
		DISP_REG_SET(cmdq, addr, val); \
	} \
} while (0)
#endif

void bdg_tx_pull_6382_reset_pin(struct mtk_dsi *dsi)
{
	struct gpio_desc *bdg_reset_gpio;
	struct device *dev = dsi->host.dev;

	DDPMSG("%s----\n", __func__);
	bdg_reset_gpio = devm_gpiod_get(dev, "bdg", GPIOD_OUT_HIGH);
	if (IS_ERR(bdg_reset_gpio))
		DDPMSG("[error] cannot get bdg reset-gpios %ld\n",
			PTR_ERR(bdg_reset_gpio));

	gpiod_set_value(bdg_reset_gpio, 1);
	udelay(10);
	gpiod_set_value(bdg_reset_gpio, 0);
	udelay(10);
	gpiod_set_value(bdg_reset_gpio, 1);
	udelay(10);
	devm_gpiod_put(dev, bdg_reset_gpio);

}

#ifdef BDG_PORTING_DBG
void bdg_tx_set_6382_reset_pin(unsigned int value)
{
	if (value)
		disp_dts_gpio_select_state(DTS_GPIO_STATE_6382_RST_OUT1);
	else
		disp_dts_gpio_select_state(DTS_GPIO_STATE_6382_RST_OUT0);
}
#endif

void bdg_tx_set_test_pattern(void)
{
	BDG_OUTREG32(NULL, TX_REG[0]->DSI_TX_SELF_PAT_CON0, 0x11);
	BDG_OUTREG32(NULL, TX_REG[0]->DSI_TX_INTSTA, 0x0);
}

void set_LDO_on(void *cmdq)
{
	unsigned int reg1, reg2, reg3;
	unsigned int timeout = 100;

	BDG_OUTREGBIT(cmdq, struct SYSREG_LDO_CTRL0_REG,
			SYS_REG->SYSREG_LDO_CTRL0, RG_PHYLDO_MASKB, 1);
	BDG_OUTREGBIT(cmdq, struct SYSREG_LDO_CTRL0_REG,
			SYS_REG->SYSREG_LDO_CTRL0, RG_LDO_TRIM_BY_EFUSE, 0);

	while (timeout) {
		reg1 = (mtk_spi_read((unsigned long)(&SYS_REG->LDO_STATUS)) & 0x300);
		DDPMSG("mt6382, %s, LDO_STATUS=0x%x\n", __func__, reg1);

		if (reg1 == 0x300)
			break;

		udelay(10);
		timeout--;
	}

	if (timeout == 0)
		return;

	reg1 = (mtk_spi_read((unsigned long)(&EFUSE->STATUS)) & 0x7fffff);
	DDPMSG("mt6382, %s, EFUSE_STATUS=0x%x\n", __func__, reg1);

	if (reg1 != 0) {
		reg1 = (mtk_spi_read((unsigned long)(&EFUSE->TRIM1)) & 0x3f);
		reg2 = (mtk_spi_read((unsigned long)(&EFUSE->TRIM2)) & 0xf);
		reg3 = (mtk_spi_read((unsigned long)(&EFUSE->TRIM3)) & 0xf);
		DDPMSG("mt6382, %s, TRIM1=0x%x, TRIM2=0x%x, TRIM3=0x%x\n",
							__func__, reg1, reg2, reg3);

		if ((reg1 != 0) | (reg2 != 0) | (reg2 != 0)) {
			BDG_OUTREGBIT(cmdq, struct SYSREG_LDO_CTRL0_REG,
					SYS_REG->SYSREG_LDO_CTRL0, RG_LDO_TRIM_BY_EFUSE, 1);
		}
	}

	BDG_OUTREGBIT(cmdq, struct SYSREG_LDO_CTRL1_REG,
			SYS_REG->SYSREG_LDO_CTRL1, RG_PHYLDO1_LP_EN, 0);
	BDG_OUTREGBIT(cmdq, struct SYSREG_LDO_CTRL1_REG,
			SYS_REG->SYSREG_LDO_CTRL1, RG_PHYLDO2_EN, 1);

}

void set_LDO_off(void *cmdq)
{
	BDG_OUTREGBIT(cmdq, struct SYSREG_LDO_CTRL1_REG,
			SYS_REG->SYSREG_LDO_CTRL1, RG_PHYLDO2_EN, 0);
	BDG_OUTREGBIT(cmdq, struct SYSREG_LDO_CTRL1_REG,
			SYS_REG->SYSREG_LDO_CTRL1, RG_PHYLDO1_LP_EN, 1);
}

void set_mtcmos_on(void *cmdq)
{
	unsigned int reg = 0;

	BDG_OUTREGBIT(cmdq, struct SYSREG_PWR_CTRL_REG,
			SYS_REG->SYSREG_PWR_CTRL, REG_PWR_ON, 1);
	BDG_OUTREGBIT(cmdq, struct SYSREG_PWR_CTRL_REG,
			SYS_REG->SYSREG_PWR_CTRL, REG_PWR_ON_2ND, 1);
	BDG_OUTREGBIT(cmdq, struct SYSREG_RST_CTRL_REG,
			SYS_REG->SYSREG_RST_CTRL, REG_SW_RST_EN_DISP_PWR_WRAP, 0);
	BDG_OUTREGBIT(cmdq, struct SYSREG_PWR_CTRL_REG,
			SYS_REG->SYSREG_PWR_CTRL, REG_PISO_EN, 0);
	BDG_OUTREGBIT(cmdq, struct SYSREG_PWR_CTRL_REG,
			SYS_REG->SYSREG_PWR_CTRL, REG_DISP_PWR_CLK_DIS, 0);
	BDG_OUTREGBIT(cmdq, struct SYSREG_PWR_CTRL_REG,
			SYS_REG->SYSREG_PWR_CTRL, REG_SYSBUF_SRAM_SLEEP_B, 1);
	BDG_OUTREGBIT(cmdq, struct SYSREG_PWR_CTRL_REG,
			SYS_REG->SYSREG_PWR_CTRL, REG_GCE_SRAM_SLEEP_B, 1);

	reg = mtk_spi_read((unsigned long)(&SYS_REG->SYSREG_PWR_CTRL));
	reg = reg | 0x00880000;
	reg = reg & 0xffbbefff;
	BDG_OUTREG32(cmdq, SYS_REG->SYSREG_PWR_CTRL, reg);
}

void set_mtcmos_off(void *cmdq)
{
	unsigned int reg = 0;

	reg = mtk_spi_read((unsigned long)(&SYS_REG->SYSREG_PWR_CTRL));
	reg = reg & 0x00000fff;
	reg = reg | 0x00661000;
	BDG_OUTREG32(cmdq, SYS_REG->SYSREG_PWR_CTRL, reg);

	reg = mtk_spi_read((unsigned long)(&SYS_REG->SYSREG_PWR_CTRL));
	reg = reg & 0xffddffff;
	BDG_OUTREG32(cmdq, SYS_REG->SYSREG_PWR_CTRL, reg);

	BDG_OUTREGBIT(cmdq, struct SYSREG_PWR_CTRL_REG,
			SYS_REG->SYSREG_PWR_CTRL, REG_DISP_PWR_CLK_DIS, 1);
	BDG_OUTREGBIT(cmdq, struct SYSREG_PWR_CTRL_REG,
			SYS_REG->SYSREG_PWR_CTRL, REG_PISO_EN, 1);
	BDG_OUTREGBIT(cmdq, struct SYSREG_RST_CTRL_REG,
			SYS_REG->SYSREG_RST_CTRL, REG_SW_RST_EN_DISP_PWR_WRAP, 1);
	BDG_OUTREGBIT(cmdq, struct SYSREG_PWR_CTRL_REG,
			SYS_REG->SYSREG_PWR_CTRL, REG_PWR_ON_2ND, 0);
	BDG_OUTREGBIT(cmdq, struct SYSREG_PWR_CTRL_REG,
			SYS_REG->SYSREG_PWR_CTRL, REG_PWR_ON, 0);
}

void set_pll_on(void *cmdq)
{
	unsigned int reg = 0;

	reg = mtk_spi_read((unsigned long)(&APMIXEDSYS->AP_PLL_CON3));
	reg = reg | 0x0000001e;
	BDG_OUTREG32(cmdq, APMIXEDSYS->AP_PLL_CON3, reg);

	reg = mtk_spi_read((unsigned long)(&APMIXEDSYS->AP_PLL_CON5));
	reg = reg | 0x00016bf0;
	BDG_OUTREG32(cmdq, APMIXEDSYS->AP_PLL_CON5, reg);

	reg = mtk_spi_read((unsigned long)(&APMIXEDSYS->AP_PLL_CON5));
	reg = reg & 0xfffdffff;
	BDG_OUTREG32(cmdq, APMIXEDSYS->AP_PLL_CON5, reg);

	reg = mtk_spi_read((unsigned long)(&APMIXEDSYS->PLLON_CON0));
	reg = reg | 0x07ffffff;
	BDG_OUTREG32(cmdq, APMIXEDSYS->PLLON_CON0, reg);

	reg = mtk_spi_read((unsigned long)(&APMIXEDSYS->PLLON_CON1));
	reg = reg | 0x07fffe00;
	BDG_OUTREG32(cmdq, APMIXEDSYS->PLLON_CON1, reg);

	reg = mtk_spi_read((unsigned long)(&APMIXEDSYS->PLLON_CON1));
	reg = reg & 0xfffffe00;
	BDG_OUTREG32(cmdq, APMIXEDSYS->PLLON_CON1, reg);

	reg = mtk_spi_read((unsigned long)(&APMIXEDSYS->MAINPLL_CON3));
	reg = reg | 0x00000001;
	BDG_OUTREG32(cmdq, APMIXEDSYS->MAINPLL_CON3, reg);

	udelay(1);

	reg = mtk_spi_read((unsigned long)(&APMIXEDSYS->MAINPLL_CON3));
	reg = reg & 0xfffffffd;
	BDG_OUTREG32(cmdq, APMIXEDSYS->MAINPLL_CON3, reg);

	BDG_OUTREG32(cmdq, APMIXEDSYS->MAINPLL_CON1, 0x800fc000);

	udelay(1);

	reg = mtk_spi_read((unsigned long)(&APMIXEDSYS->MAINPLL_CON0));
	reg = reg | 0x00000001;
	BDG_OUTREG32(cmdq, APMIXEDSYS->MAINPLL_CON0, reg);

	udelay(20);

	reg = mtk_spi_read((unsigned long)(&APMIXEDSYS->MAINPLL_CON0));
	reg = reg | 0x00800001;
	BDG_OUTREG32(cmdq, APMIXEDSYS->MAINPLL_CON0, reg);
}

void set_pll_off(void *cmdq)
{
	unsigned int reg = 0;

	reg = mtk_spi_read((unsigned long)(&APMIXEDSYS->AP_PLL_CON3));
	reg = reg | 0x00000000;
	BDG_OUTREG32(cmdq, APMIXEDSYS->AP_PLL_CON3, reg);

	reg = mtk_spi_read((unsigned long)(&APMIXEDSYS->AP_PLL_CON3));
	reg = reg & 0xffffffe1;
	BDG_OUTREG32(cmdq, APMIXEDSYS->AP_PLL_CON3, reg);

	reg = mtk_spi_read((unsigned long)(&APMIXEDSYS->AP_PLL_CON5));
	reg = reg | 0x000020a0;
	BDG_OUTREG32(cmdq, APMIXEDSYS->AP_PLL_CON5, reg);

	reg = mtk_spi_read((unsigned long)(&APMIXEDSYS->AP_PLL_CON5));
	reg = reg & 0xfffcb4af;
	BDG_OUTREG32(cmdq, APMIXEDSYS->AP_PLL_CON5, reg);

	reg = mtk_spi_read((unsigned long)(&APMIXEDSYS->PLLON_CON0));
	reg = reg | 0x07ffffff;
	BDG_OUTREG32(cmdq, APMIXEDSYS->PLLON_CON0, reg);

	reg = mtk_spi_read((unsigned long)(&APMIXEDSYS->PLLON_CON1));
	reg = reg | 0x07fffe00;
	BDG_OUTREG32(cmdq, APMIXEDSYS->PLLON_CON1, reg);

	reg = mtk_spi_read((unsigned long)(&APMIXEDSYS->PLLON_CON1));
	reg = reg & 0xfffffe00;
	BDG_OUTREG32(cmdq, APMIXEDSYS->PLLON_CON1, reg);

	reg = mtk_spi_read((unsigned long)(&APMIXEDSYS->MAINPLL_CON0));
	reg = reg | 0x00000000;
	BDG_OUTREG32(cmdq, APMIXEDSYS->MAINPLL_CON0, reg);

	reg = mtk_spi_read((unsigned long)(&APMIXEDSYS->MAINPLL_CON0));
	reg = reg & 0xff7ffffe;
	BDG_OUTREG32(cmdq, APMIXEDSYS->MAINPLL_CON0, reg);

	udelay(1);

	reg = mtk_spi_read((unsigned long)(&APMIXEDSYS->MAINPLL_CON3));
	reg = reg | 0x00000003;
	BDG_OUTREG32(cmdq, APMIXEDSYS->MAINPLL_CON3, reg);

	udelay(1);

	reg = mtk_spi_read((unsigned long)(&APMIXEDSYS->MAINPLL_CON3));
	reg = reg & 0xfffffffe;
	BDG_OUTREG32(cmdq, APMIXEDSYS->MAINPLL_CON3, reg);
}

void ana_macro_on(void *cmdq)
{
	unsigned int reg = 0;

	//select pll power on `MAINPLL_CON3
	BDG_OUTREG32(cmdq, APMIXEDSYS->MAINPLL_CON3, 3);
	udelay(1);
//	DDPMSG("mt6382, %s, delay 1us\n", __func__);
	//select pll iso enable `MAINPLL_CON3
	BDG_OUTREG32(cmdq, APMIXEDSYS->MAINPLL_CON3, 1);
	BDG_OUTREG32(cmdq, APMIXEDSYS->MAINPLL_CON0, 0xff000000);
	BDG_OUTREG32(cmdq, APMIXEDSYS->MAINPLL_CON1, 0x000fc000);
	udelay(2);
//	DDPMSG("mt6382, %s, delay 2us\n", __func__);
	//reset setting `MAINPLL_CON0
	BDG_OUTREG32(cmdq, APMIXEDSYS->MAINPLL_CON0, 0xff000001);
	udelay(25);
//	DDPMSG("mt6382, %s, delay 25us\n", __func__);
	BDG_OUTREG32(cmdq, APMIXEDSYS->MAINPLL_CON0, 0xff800001);

	BDG_OUTREG32(cmdq, TOPCKGEN->CLK_CFG_0_CLR, 0xffffffff);

	/* set clock source
	 * bit 16-17 is display mm clk 1(270m)/2(405m)/3(540m)
	 * dsc_on:vact * hact * vrefresh * (vtotal / vact) * bubble_ratio
	 */
	switch (MM_CLK) {
	case 546:
		DDPMSG("%s, 6382 mmclk 546M\n", __func__);
		reg = (3 << 24) | (3 << 16) | (1 << 8) | (1 << 0); //540M
		break;
	case 405:
		DDPMSG("%s, 6382 mmclk 405M\n", __func__);
		reg = (3 << 24) | (2 << 16) | (1 << 8) | (1 << 0); //405M for 120Hz
		break;
	case 270:
		DDPMSG("%s, 6382 mmclk 270M\n", __func__);
		reg = (3 << 24) | (1 << 16) | (1 << 8) | (1 << 0); //270M for 90Hz
		break;
	default:
		DDPMSG("%s, 6382 mmclk default 546M\n", __func__);
		reg = (3 << 24) | (3 << 16) | (1 << 8) | (1 << 0); //540M
		break;
	}
	BDG_OUTREG32(cmdq, TOPCKGEN->CLK_CFG_0_SET, reg);
	//config update
	reg = (1 << 4) | (1 << 3) | (1 << 1) | (1 << 0);
	BDG_OUTREG32(cmdq, TOPCKGEN->CLK_CFG_UPDATE, reg);

	udelay(6);
//	DDPMSG("mt6382, %s, delay 6us\n", __func__);

}

void ana_macro_off(void *cmdq)
{
	unsigned int reg = 0;

//	ANA_MIPI_DSI_OFF ();
	BDG_OUTREG32(cmdq, DSI2_REG->DSI2_DEVICE_PHY_SHUTDOWNZ_OS, 0);
	BDG_OUTREG32(cmdq, DSI2_REG->DSI2_DEVICE_PHY_RSTZ_OS, 0);

	//config update
	reg = (3 << 24) | (3 << 16) | (1 << 8) | (3 << 0);
	BDG_OUTREG32(cmdq, TOPCKGEN->CLK_CFG_0_CLR, reg);
	//config update
	reg = (1 << 4) | (1 << 3) | (1 << 1) | (1 << 0);
	BDG_OUTREG32(cmdq, TOPCKGEN->CLK_CFG_UPDATE, reg);

//	#5000ns; //wait clk mux stable
	udelay(6);
//	spis_low_speed_cfg();
//	DDPMSG("mt6382, %s, delay 6us\n", __func__);
	set_pll_off(cmdq);
}

void set_topck_on(void *cmdq)
{
	unsigned int reg = 0;

	reg = mtk_spi_read((unsigned long)(&TOPCKGEN->CLK_MODE));
	reg = reg & 0xfffffffd;
	BDG_OUTREG32(cmdq, TOPCKGEN->CLK_MODE, reg);

	reg = mtk_spi_read((unsigned long)(&TOPCKGEN->CLK_CFG_0));
	reg = reg & 0xdf7f7f7f;
	BDG_OUTREG32(cmdq, TOPCKGEN->CLK_CFG_0, reg);

}

void set_topck_off(void *cmdq)
{
	unsigned int reg = 0;

	//select clock source
	BDG_OUTREG32(cmdq, TOPCKGEN->CLK_CFG_0_CLR, 0x03030103);
	BDG_OUTREG32(cmdq, TOPCKGEN->CLK_CFG_0_SET, 0);
	//config update
	BDG_OUTREG32(cmdq, TOPCKGEN->CLK_CFG_UPDATE, 0x0000001b);

//	#5000ns; //wait clk mux stable
	udelay(6);
//	DDPMSG("mt6382, %s, delay 6us\n", __func__);
//	spis_low_speed_cfg();
//	set_pll_off(cmdq);

	BDG_OUTREG32(cmdq, TOPCKGEN->CLK_CFG_0_CLR, 0x00808080);
	BDG_OUTREG32(cmdq, TOPCKGEN->CLK_CFG_0_SET, 0x00808080);
	BDG_OUTREG32(cmdq, TOPCKGEN->CLK_CFG_UPDATE, 0x0000001b);
	reg = mtk_spi_read((unsigned long)(&TOPCKGEN->CLK_MODE));
	reg = reg | 0x00000001;
	BDG_OUTREG32(cmdq, TOPCKGEN->CLK_MODE, reg);
}

void set_subsys_on(void *cmdq)
{
	unsigned int reg = 0;

	reg = mtk_spi_read((unsigned long)(&GCE_REG->GCE_CTL_INT0));
	reg = reg & 0x0000ffff;
	BDG_OUTREG32(cmdq, GCE_REG->GCE_CTL_INT0, reg);

	//Clear CG
	BDG_OUTREG32(cmdq, DISPSYS_REG->MMSYS_CG_CON0, 0);
	//Turn off all DCM, Clock on
	reg = mtk_spi_read((unsigned long)(&EFUSE->DCM_ON));
	reg = reg & 0xfffffff8;
	BDG_OUTREG32(cmdq, EFUSE->DCM_ON, reg);

	reg = mtk_spi_read((unsigned long)(&GCE_REG->GCE_CTL_INT0));
	reg = reg | 0x0000ffff;
	BDG_OUTREG32(cmdq, GCE_REG->GCE_CTL_INT0, reg);

	BDG_OUTREG32(cmdq, DISPSYS_REG->MMSYS_HW_DCM_1ST_DIS0, 0xffffffff);
	BDG_OUTREG32(cmdq, DISPSYS_REG->MMSYS_HW_DCM_2ND_DIS0, 0xffffffff);

	// enable 26m clock
	BDG_OUTREG32(cmdq, SYS_REG->DISP_MISC1, 1);
}

void set_subsys_off(void *cmdq)
{
	unsigned int reg = 0;

	reg = mtk_spi_read((unsigned long)(&GCE_REG->GCE_CTL_INT0));
	reg = reg | 0x00010000;
	BDG_OUTREG32(cmdq, GCE_REG->GCE_CTL_INT0, reg);

	reg = mtk_spi_read((unsigned long)(&GCE_REG->GCE_CTL_INT0));
	reg = reg & 0x0001ffff;
	BDG_OUTREG32(cmdq, GCE_REG->GCE_CTL_INT0, reg);

	//Set CG
	BDG_OUTREG32(cmdq, DISPSYS_REG->MMSYS_CG_CON0, 0xffffffff);
	//Turn on all DCM, Clock off
	reg = mtk_spi_read((unsigned long)(&EFUSE->DCM_ON));
	reg = reg | 0x00000007;
	BDG_OUTREG32(cmdq, EFUSE->DCM_ON, reg);

	reg = mtk_spi_read((unsigned long)(&GCE_REG->GCE_CTL_INT0));
	reg = reg | 0x0000ffff;
	BDG_OUTREG32(cmdq, GCE_REG->GCE_CTL_INT0, reg);

	BDG_OUTREG32(cmdq, DISPSYS_REG->MMSYS_HW_DCM_1ST_DIS0, 0);
	BDG_OUTREG32(cmdq, DISPSYS_REG->MMSYS_HW_DCM_2ND_DIS0, 0);
}

void set_ana_mipi_dsi_off(void *cmdq)
{
	unsigned int timeout = 5000;

	//Tx mac enter ulps sequence:
	//Disable clock lane high speed mode
	BDG_OUTREGBIT(cmdq, struct DSI_TX_PHY_LCCON_REG,
		TX_REG[0]->DSI_TX_PHY_LCCON, LC_HS_TX_EN, 0); //[0]: lc_hstx_en

	//Disable all lane ultra-low power state mode
	BDG_OUTREGBIT(cmdq, struct DSI_TX_PHY_LD0CON_REG,
		TX_REG[0]->DSI_TX_PHY_LD0CON, L0_ULPM_EN, 0); //[0]: lc_hstx_en

	BDG_OUTREGBIT(cmdq, struct DSI_TX_PHY_LCCON_REG,
		TX_REG[0]->DSI_TX_PHY_LCCON, LC_ULPM_EN, 0); //[1]: lc_ulpm_en

#ifdef BDG_PORTING_DBG
    //Enable SLEEPIN_ULPS_INT_EN
	BDG_OUTREGBIT(cmdq, struct DSI_TX_INTEN_REG,
		TX_REG[0]->DSI_TX_INTEN, SLEEPIN_ULPS_INT_EN, 1); //[15]: sleepin_ulps_int_en
#endif

    //Enalbe all lane ultra-low power state mode
	BDG_OUTREGBIT(cmdq, struct DSI_TX_PHY_LD0CON_REG,
		TX_REG[0]->DSI_TX_PHY_LD0CON, LX_ULPM_AS_L0, 1); //[3]: lx_ulpm_as_l0
	BDG_OUTREGBIT(cmdq, struct DSI_TX_PHY_LD0CON_REG,
		TX_REG[0]->DSI_TX_PHY_LD0CON, L0_ULPM_EN, 1); //[1]: l0_ulpm_en
	BDG_OUTREGBIT(cmdq, struct DSI_TX_PHY_LCCON_REG,
		TX_REG[0]->DSI_TX_PHY_LCCON, LC_ULPM_EN, 1); //[1]: lc_ulpm_ens


	//Wait sleep in irq and clear irq state
	while ((mtk_spi_read((unsigned long)(&TX_REG[0]->DSI_TX_INTSTA)) & 0x8000) != 0x8000) {
		udelay(1);
		timeout--;

		if (timeout == 0) {
			DDPMSG("%s, wait timeout!\n", __func__);
			break;
		}
	}

#ifdef BDG_PORTING_DBG
    //disable SLEEPIN_ULPS_INT_EN
	BDG_OUTREGBIT(cmdq, struct DSI_TX_INTEN_REG,
		TX_REG[0]->DSI_TX_INTEN, SLEEPIN_ULPS_INT_EN, 0); //[15]: sleepin_ulps_int_en
#endif

    //mipi_dsi_tx_phy_sw_lp00_enable
	BDG_OUTREGBIT(cmdq, struct MIPI_TX_SW_LPTX_PRE_OE_REG,
		MIPI_TX_REG->MIPI_TX_D2_SW_LPTX_PRE_OE, DSI_SW_LPTX_PRE_OE, 0); //[0]
	BDG_OUTREGBIT(cmdq, struct MIPI_TX_SW_LPTX_OE_REG,
		MIPI_TX_REG->MIPI_TX_D2_SW_LPTX_OE, DSI_SW_LPTX_OE, 0); //[0]: dsi_d2_sw_lptx_oe
	BDG_OUTREGBIT(cmdq, struct MIPI_TX_SW_LPTX_DP_REG,
		MIPI_TX_REG->MIPI_TX_D2_SW_LPTX_DP, DSI_SW_LPTX_DP, 0); //[0]: dsi_d2_sw_lptx_dp
	BDG_OUTREGBIT(cmdq, struct MIPI_TX_SW_LPTX_DN_REG,
		MIPI_TX_REG->MIPI_TX_D2_SW_LPTX_DN, DSI_SW_LPTX_DN, 0); //[0]: dsi_d2_sw_lptx_dn

	BDG_OUTREGBIT(cmdq, struct MIPI_TX_SW_LPTX_PRE_OE_REG,
		MIPI_TX_REG->MIPI_TX_D0_SW_LPTX_PRE_OE, DSI_SW_LPTX_PRE_OE, 0); //[0]
	BDG_OUTREGBIT(cmdq, struct MIPI_TX_SW_LPTX_OE_REG,
		MIPI_TX_REG->MIPI_TX_D0_SW_LPTX_OE, DSI_SW_LPTX_OE, 0); //[0]: dsi_d0_sw_lptx_oe
	BDG_OUTREGBIT(cmdq, struct MIPI_TX_SW_LPTX_DP_REG,
		MIPI_TX_REG->MIPI_TX_D0_SW_LPTX_DP, DSI_SW_LPTX_DP, 0); //[0]: dsi_d0_sw_lptx_dp
	BDG_OUTREGBIT(cmdq, struct MIPI_TX_SW_LPTX_DN_REG,
		MIPI_TX_REG->MIPI_TX_D0_SW_LPTX_DN, DSI_SW_LPTX_DN, 0); //[0]: dsi_d0_sw_lptx_dn

	BDG_OUTREGBIT(cmdq, struct MIPI_TX_SW_LPTX_PRE_OE_REG,
		MIPI_TX_REG->MIPI_TX_CK_SW_LPTX_PRE_OE, DSI_SW_LPTX_PRE_OE, 0); //[0]
	BDG_OUTREGBIT(cmdq, struct MIPI_TX_SW_LPTX_OE_REG,
		MIPI_TX_REG->MIPI_TX_CK_SW_LPTX_OE, DSI_SW_LPTX_OE, 0); //[0]: dsi_ck_sw_lptx_oe
	BDG_OUTREGBIT(cmdq, struct MIPI_TX_SW_LPTX_DP_REG,
		MIPI_TX_REG->MIPI_TX_CK_SW_LPTX_DP, DSI_SW_LPTX_DP, 0); //[0]: dsi_ck_sw_lptx_dp
	BDG_OUTREGBIT(cmdq, struct MIPI_TX_SW_LPTX_DN_REG,
		MIPI_TX_REG->MIPI_TX_CK_SW_LPTX_DN, DSI_SW_LPTX_DN, 0); //[0]: dsi_ck_sw_lptx_dn

	BDG_OUTREGBIT(cmdq, struct MIPI_TX_SW_LPTX_PRE_OE_REG,
		MIPI_TX_REG->MIPI_TX_D1_SW_LPTX_PRE_OE, DSI_SW_LPTX_PRE_OE, 0); //[0]
	BDG_OUTREGBIT(cmdq, struct MIPI_TX_SW_LPTX_OE_REG,
		MIPI_TX_REG->MIPI_TX_D1_SW_LPTX_OE, DSI_SW_LPTX_OE, 0); //[0]: dsi_d1_sw_lptx_oe
	BDG_OUTREGBIT(cmdq, struct MIPI_TX_SW_LPTX_DP_REG,
		MIPI_TX_REG->MIPI_TX_D1_SW_LPTX_DP, DSI_SW_LPTX_DP, 0); //[0]: dsi_d1_sw_lptx_dp
	BDG_OUTREGBIT(cmdq, struct MIPI_TX_SW_LPTX_DN_REG,
		MIPI_TX_REG->MIPI_TX_D1_SW_LPTX_DN, DSI_SW_LPTX_DN, 0); //[0]: dsi_d1_sw_lptx_dn

	BDG_OUTREGBIT(cmdq, struct MIPI_TX_SW_LPTX_PRE_OE_REG,
		MIPI_TX_REG->MIPI_TX_D3_SW_LPTX_PRE_OE, DSI_SW_LPTX_PRE_OE, 0); //[0]
	BDG_OUTREGBIT(cmdq, struct MIPI_TX_SW_LPTX_OE_REG,
		MIPI_TX_REG->MIPI_TX_D3_SW_LPTX_OE, DSI_SW_LPTX_OE, 0); //[0]: dsi_d3_sw_lptx_oe
	BDG_OUTREGBIT(cmdq, struct MIPI_TX_SW_LPTX_DP_REG,
		MIPI_TX_REG->MIPI_TX_D3_SW_LPTX_DP, DSI_SW_LPTX_DP, 0); //[0]: dsi_d3_sw_lptx_dp
	BDG_OUTREGBIT(cmdq, struct MIPI_TX_SW_LPTX_DN_REG,
		MIPI_TX_REG->MIPI_TX_D3_SW_LPTX_DN, DSI_SW_LPTX_DN, 0); //[0]: dsi_d3_sw_lptx_dn

	BDG_OUTREGBIT(cmdq, struct MIPI_TX_SW_CTL_EN_REG,
		MIPI_TX_REG->MIPI_TX_D2_SW_CTL_EN, DSI_SW_CTL_EN, 1); //[0]: dsi_d2_sw_ctl_en
	BDG_OUTREGBIT(cmdq, struct MIPI_TX_SW_CTL_EN_REG,
		MIPI_TX_REG->MIPI_TX_D0_SW_CTL_EN, DSI_SW_CTL_EN, 1); //[0]: dsi_d0_sw_ctl_en
	BDG_OUTREGBIT(cmdq, struct MIPI_TX_SW_CTL_EN_REG,
		MIPI_TX_REG->MIPI_TX_CK_SW_CTL_EN, DSI_SW_CTL_EN, 1); //[0]: dsi_ck_sw_ctl_en
	BDG_OUTREGBIT(cmdq, struct MIPI_TX_SW_CTL_EN_REG,
		MIPI_TX_REG->MIPI_TX_D1_SW_CTL_EN, DSI_SW_CTL_EN, 1); //[0]: dsi_d1_sw_ctl_en
	BDG_OUTREGBIT(cmdq, struct MIPI_TX_SW_CTL_EN_REG,
		MIPI_TX_REG->MIPI_TX_D3_SW_CTL_EN, DSI_SW_CTL_EN, 1); //[0]: dsi_d3_sw_ctl_en

    //Clear lane_num when enter ulps
	BDG_OUTREGBIT(cmdq, struct DSI_TX_TXRX_CON_REG,
		TX_REG[0]->DSI_TX_TXRX_CON, LANE_NUM, 0);

	//tx_phy_disalbe:
	BDG_OUTREGBIT(cmdq, struct MIPI_TX_PLL_CON1_REG,
		MIPI_TX_REG->MIPI_TX_PLL_CON1, RG_DSI_PLL_EN, 0);

	//pll_sdm_iso_en = 1, pll_sdm_pwr_on = 0
	BDG_OUTREGBIT(cmdq, struct MIPI_TX_PLL_PWR_REG,
		MIPI_TX_REG->MIPI_TX_PLL_PWR, AD_DSI_PLL_SDM_ISO_EN, 1); //[1]
	BDG_OUTREGBIT(cmdq, struct MIPI_TX_PLL_PWR_REG,
		MIPI_TX_REG->MIPI_TX_PLL_PWR, AD_DSI_PLL_SDM_PWR_ON, 0); //[0]

	// TIEL_SEL=1
	BDG_OUTREGBIT(cmdq, struct MIPI_TX_LANE_CON_REG,
		MIPI_TX_REG->MIPI_TX_LANE_CON, RG_DSI_PAD_TIEL_SEL, 1);

	// BG_LPF_EN=0
	BDG_OUTREGBIT(cmdq, struct MIPI_TX_LANE_CON_REG,
		MIPI_TX_REG->MIPI_TX_LANE_CON, RG_DSI_BG_LPF_EN, 0);

	// BG_CORE_EN=0
	BDG_OUTREGBIT(cmdq, struct MIPI_TX_LANE_CON_REG,
		MIPI_TX_REG->MIPI_TX_LANE_CON, RG_DSI_BG_CORE_EN, 0);
}


int dsi_get_pcw(int data_rate, int pcw_ratio)
{
	int pcw, tmp, pcw_floor;

	/**
	 * PCW bit 24~30 = floor(pcw)
	 * PCW bit 16~23 = (pcw - floor(pcw))*256
	 * PCW bit 8~15 = (pcw*256 - floor(pcw)*256)*256
	 * PCW bit 0~7 = (pcw*256*256 - floor(pcw)*256*256)*256
	 */
	pcw = data_rate * pcw_ratio / 26;
	pcw_floor = data_rate * pcw_ratio % 26;
	tmp = ((pcw & 0xFF) << 24) | (((256 * pcw_floor / 26) & 0xFF) << 16) |
		(((256 * (256 * pcw_floor % 26) / 26) & 0xFF) << 8) |
		((256 * (256 * (256 * pcw_floor % 26) % 26) / 26) & 0xFF);

	return tmp;
}

int bdg_mipi_tx_dphy_clk_setting(enum DISP_BDG_ENUM module,
				 void *cmdq, struct mtk_dsi *dsi)
{
	int i = 0;
	unsigned int j = 0;
	unsigned int data_Rate;
	unsigned int pcw_ratio = 0;
	unsigned int posdiv = 0;
	unsigned int prediv = 0;
	unsigned int delta1 = 2; /* Delta1 is SSC range, default is 0%~-5% */
	unsigned int pdelta1 = 0;
	struct mtk_panel_params *params = dsi->ext->params;
	enum MIPITX_PHY_LANE_SWAP *swap_base;
	enum MIPI_TX_PAD_VALUE pad_mapping[MIPITX_PHY_LANE_NUM] = {
					PAD_D0P, PAD_D1P, PAD_D2P,
					PAD_D3P, PAD_CKP, PAD_CKP};

	data_Rate = dsi->bdg_data_rate;

	DDPINFO("%s, data_Rate=%d\n",	__func__, data_Rate);

	/* DPHY SETTING */
	/* MIPITX lane swap setting */

	for (i = DSI_MODULE_BEGIN(module); i <= DSI_MODULE_END(module); i++) {
		/* step 0 MIPITX lane swap setting */
		swap_base = params->lane_swap[i];
		DDPINFO(
			"MIPITX Lane Swap mapping: 0=%d|1=%d|2=%d|3=%d|CK=%d|RX=%d\n",
			 swap_base[MIPITX_PHY_LANE_0],
			 swap_base[MIPITX_PHY_LANE_1],
			 swap_base[MIPITX_PHY_LANE_2],
			 swap_base[MIPITX_PHY_LANE_3],
			 swap_base[MIPITX_PHY_LANE_CK],
			 swap_base[MIPITX_PHY_LANE_RX]);

		if (unlikely(params->lane_swap_en)) {
			DDPINFO("MIPITX Lane Swap Enabled for DSI Port %d\n",
				 i);
			DDPINFO(
				"MIPITX Lane Swap mapping: %d|%d|%d|%d|%d|%d\n",
				 swap_base[MIPITX_PHY_LANE_0],
				 swap_base[MIPITX_PHY_LANE_1],
				 swap_base[MIPITX_PHY_LANE_2],
				 swap_base[MIPITX_PHY_LANE_3],
				 swap_base[MIPITX_PHY_LANE_CK],
				 swap_base[MIPITX_PHY_LANE_RX]);

			/* CKMODE_EN */
			for (j = MIPITX_PHY_LANE_0; j < MIPITX_PHY_LANE_CK;
			     j++) {
				if (params->lane_swap[i][j] ==
				    MIPITX_PHY_LANE_CK)
					break;
			}
			switch (j) {
			case MIPITX_PHY_LANE_0:
				BDG_OUTREGBIT(cmdq, struct MIPI_TX_CKMODE_EN_REG,
						MIPI_TX_REG->MIPI_TX_D0_CKMODE_EN,
						DSI_CKMODE_EN, 1);
				break;
			case MIPITX_PHY_LANE_1:
				BDG_OUTREGBIT(cmdq, struct MIPI_TX_CKMODE_EN_REG,
						MIPI_TX_REG->MIPI_TX_D1_CKMODE_EN,
						DSI_CKMODE_EN, 1);
				break;
			case MIPITX_PHY_LANE_2:
				BDG_OUTREGBIT(cmdq, struct MIPI_TX_CKMODE_EN_REG,
						MIPI_TX_REG->MIPI_TX_D2_CKMODE_EN,
						DSI_CKMODE_EN, 1);
				break;
			case MIPITX_PHY_LANE_3:
				BDG_OUTREGBIT(cmdq, struct MIPI_TX_CKMODE_EN_REG,
						MIPI_TX_REG->MIPI_TX_D3_CKMODE_EN,
						DSI_CKMODE_EN, 1);
				break;
			case MIPITX_PHY_LANE_CK:
				BDG_OUTREGBIT(cmdq, struct MIPI_TX_CKMODE_EN_REG,
						MIPI_TX_REG->MIPI_TX_CK_CKMODE_EN,
						DSI_CKMODE_EN, 1);
				break;
			default:
				break;
			}

			/* LANE_0 */
			BDG_OUTREGBIT(cmdq, struct MIPI_TX_PHY_SEL0_REG,
					MIPI_TX_REG->MIPI_TX_PHY_SEL0,
					MIPI_TX_PHY0_SEL,
					pad_mapping[swap_base[MIPITX_PHY_LANE_0]]);
			BDG_OUTREGBIT(cmdq, struct MIPI_TX_PHY_SEL0_REG,
					MIPI_TX_REG->MIPI_TX_PHY_SEL0,
					MIPI_TX_PHY1AB_SEL,
					pad_mapping[swap_base[MIPITX_PHY_LANE_0]] + 1);

			/* LANE_1 */
			BDG_OUTREGBIT(cmdq, struct MIPI_TX_PHY_SEL0_REG,
					MIPI_TX_REG->MIPI_TX_PHY_SEL0,
					MIPI_TX_PHY1_SEL,
					pad_mapping[swap_base[MIPITX_PHY_LANE_1]]);
			BDG_OUTREGBIT(cmdq, struct MIPI_TX_PHY_SEL1_REG,
					MIPI_TX_REG->MIPI_TX_PHY_SEL1,
					MIPI_TX_PHY2BC_SEL,
					pad_mapping[swap_base[MIPITX_PHY_LANE_1]] + 1);

			/* LANE_2 */
			BDG_OUTREGBIT(cmdq, struct MIPI_TX_PHY_SEL0_REG,
					MIPI_TX_REG->MIPI_TX_PHY_SEL0,
					MIPI_TX_PHY2_SEL,
					pad_mapping[swap_base[MIPITX_PHY_LANE_2]]);
			BDG_OUTREGBIT(cmdq, struct MIPI_TX_PHY_SEL0_REG,
					MIPI_TX_REG->MIPI_TX_PHY_SEL0,
					MIPI_TX_CPHY0BC_SEL,
					pad_mapping[swap_base[MIPITX_PHY_LANE_2]] + 1);

			/* LANE_3 */
			BDG_OUTREGBIT(cmdq, struct MIPI_TX_PHY_SEL1_REG,
					MIPI_TX_REG->MIPI_TX_PHY_SEL1,
					MIPI_TX_PHY3_SEL,
					pad_mapping[swap_base[MIPITX_PHY_LANE_3]]);
			BDG_OUTREGBIT(cmdq, struct MIPI_TX_PHY_SEL1_REG,
					MIPI_TX_REG->MIPI_TX_PHY_SEL1,
					MIPI_TX_CPHYXXX_SEL,
					pad_mapping[swap_base[MIPITX_PHY_LANE_3]] + 1);

			/* CK LANE */
			BDG_OUTREGBIT(cmdq, struct MIPI_TX_PHY_SEL0_REG,
					MIPI_TX_REG->MIPI_TX_PHY_SEL0,
					MIPI_TX_PHYC_SEL,
					pad_mapping[swap_base[MIPITX_PHY_LANE_CK]]);
			BDG_OUTREGBIT(cmdq, struct MIPI_TX_PHY_SEL0_REG,
					MIPI_TX_REG->MIPI_TX_PHY_SEL0,
					MIPI_TX_CPHY1CA_SEL,
					pad_mapping[swap_base[MIPITX_PHY_LANE_CK]] + 1);

			/* LPRX SETTING */
			BDG_OUTREGBIT(cmdq, struct MIPI_TX_PHY_SEL1_REG,
					MIPI_TX_REG->MIPI_TX_PHY_SEL1,
					MIPI_TX_LPRX0AB_SEL,
					pad_mapping[swap_base[MIPITX_PHY_LANE_RX]]);
			BDG_OUTREGBIT(cmdq, struct MIPI_TX_PHY_SEL1_REG,
					MIPI_TX_REG->MIPI_TX_PHY_SEL1,
					MIPI_TX_LPRX0BC_SEL,
					pad_mapping[swap_base[MIPITX_PHY_LANE_RX]] + 1);

			/* HS_DATA SETTING */
			BDG_OUTREGBIT(cmdq, struct MIPI_TX_PHY_SEL2_REG,
					MIPI_TX_REG->MIPI_TX_PHY_SEL2,
					MIPI_TX_PHY2_HSDATA_SEL,
					pad_mapping[swap_base[MIPITX_PHY_LANE_2]]);
			BDG_OUTREGBIT(cmdq, struct MIPI_TX_PHY_SEL2_REG,
					MIPI_TX_REG->MIPI_TX_PHY_SEL2,
					MIPI_TX_PHY0_HSDATA_SEL,
					pad_mapping[swap_base[MIPITX_PHY_LANE_0]]);
			BDG_OUTREGBIT(cmdq, struct MIPI_TX_PHY_SEL2_REG,
					MIPI_TX_REG->MIPI_TX_PHY_SEL2,
					MIPI_TX_PHYC_HSDATA_SEL,
					pad_mapping[swap_base[MIPITX_PHY_LANE_CK]]);
			BDG_OUTREGBIT(cmdq, struct MIPI_TX_PHY_SEL2_REG,
					MIPI_TX_REG->MIPI_TX_PHY_SEL2,
					MIPI_TX_PHY1_HSDATA_SEL,
					pad_mapping[swap_base[MIPITX_PHY_LANE_1]]);
			BDG_OUTREGBIT(cmdq, struct MIPI_TX_PHY_SEL3_REG,
					MIPI_TX_REG->MIPI_TX_PHY_SEL3,
					MIPI_TX_PHY3_HSDATA_SEL,
					pad_mapping[swap_base[MIPITX_PHY_LANE_3]]);
		} else {
			BDG_OUTREGBIT(cmdq, struct MIPI_TX_CKMODE_EN_REG,
					MIPI_TX_REG->MIPI_TX_CK_CKMODE_EN,
					DSI_CKMODE_EN, 1);
		}
	}

//	refill_mipitx_impedance(NULL);

	/* MIPI INIT */
	for (i = DSI_MODULE_BEGIN(module); i <= DSI_MODULE_END(module); i++) {
		unsigned int tmp = 0;

		/* step 0: RG_DSI0_PLL_IBIAS = 0*/
		BDG_OUTREG32(cmdq, MIPI_TX_REG->MIPI_TX_PLL_CON4, 0x00FF12E0);
		/* BG_LPF_EN / BG_CORE_EN */
		BDG_OUTREG32(cmdq, MIPI_TX_REG->MIPI_TX_LANE_CON, 0x3FFF0180);
		udelay(2);
		/* BG_LPF_EN=1,TIEL_SEL=0 */
		BDG_OUTREG32(cmdq, MIPI_TX_REG->MIPI_TX_LANE_CON, 0x3FFF0080);
//		if (atomic_read(&dsi_idle_flg) == 0) {
			/* Switch OFF each Lane */
			BDG_OUTREGBIT(cmdq, struct MIPI_TX_SW_CTL_EN_REG,
					MIPI_TX_REG->MIPI_TX_D0_SW_CTL_EN,
					DSI_SW_CTL_EN, 0);
			BDG_OUTREGBIT(cmdq, struct MIPI_TX_SW_CTL_EN_REG,
					MIPI_TX_REG->MIPI_TX_D1_SW_CTL_EN,
					DSI_SW_CTL_EN, 0);
			BDG_OUTREGBIT(cmdq, struct MIPI_TX_SW_CTL_EN_REG,
					MIPI_TX_REG->MIPI_TX_D2_SW_CTL_EN,
					DSI_SW_CTL_EN, 0);
			BDG_OUTREGBIT(cmdq, struct MIPI_TX_SW_CTL_EN_REG,
					MIPI_TX_REG->MIPI_TX_D3_SW_CTL_EN,
					DSI_SW_CTL_EN, 0);
			BDG_OUTREGBIT(cmdq, struct MIPI_TX_SW_CTL_EN_REG,
					MIPI_TX_REG->MIPI_TX_CK_SW_CTL_EN,
					DSI_SW_CTL_EN, 0);
//		}

		/* step 1: SDM_RWR_ON / SDM_ISO_EN */
		BDG_OUTREGBIT(cmdq, struct MIPI_TX_PLL_PWR_REG,
				MIPI_TX_REG->MIPI_TX_PLL_PWR,
				AD_DSI_PLL_SDM_PWR_ON, 1);
		udelay(2); /* 1us */
		BDG_OUTREGBIT(cmdq, struct MIPI_TX_PLL_PWR_REG,
				MIPI_TX_REG->MIPI_TX_PLL_PWR,
				AD_DSI_PLL_SDM_ISO_EN, 0);

		if (data_Rate > 2500) {
			DDPINFO("mipitx Data Rate exceed limitation(%d)\n",
				    data_Rate);
			return -2;
		} else if (data_Rate >= 2000) { /* 2G ~ 2.5G */
			pcw_ratio = 1;
			posdiv    = 0;
			prediv    = 0;
		} else if (data_Rate >= 1000) { /* 1G ~ 2G */
			pcw_ratio = 2;
			posdiv    = 1;
			prediv    = 0;
		} else if (data_Rate >= 500) { /* 500M ~ 1G */
			pcw_ratio = 4;
			posdiv    = 2;
			prediv    = 0;
		} else if (data_Rate > 250) { /* 250M ~ 500M */
			pcw_ratio = 8;
			posdiv    = 3;
			prediv    = 0;
		} else if (data_Rate >= 125) { /* 125M ~ 250M */
			pcw_ratio = 16;
			posdiv    = 4;
			prediv    = 0;
		} else {
			DDPINFO("dataRate is too low(%d)\n", data_Rate);
			return -3;
		}

		/* step 3 */
		/* PLL PCW config */
		tmp = dsi_get_pcw(data_Rate, pcw_ratio);
		BDG_OUTREG32(cmdq, MIPI_TX_REG->MIPI_TX_PLL_CON0, tmp);
		BDG_OUTREGBIT(cmdq, struct MIPI_TX_PLL_CON1_REG,
				MIPI_TX_REG->MIPI_TX_PLL_CON1,
				RG_DSI_PLL_POSDIV, posdiv);

		/* SSC config */
		if (dsi->ext->params->bdg_ssc_disable != 1) {
			BDG_OUTREGBIT(cmdq, struct MIPI_TX_PLL_CON2_REG,
					MIPI_TX_REG->MIPI_TX_PLL_CON2,
					RG_DSI_PLL_SDM_SSC_PH_INIT, 1);
			BDG_OUTREGBIT(cmdq, struct MIPI_TX_PLL_CON2_REG,
					MIPI_TX_REG->MIPI_TX_PLL_CON2,
					RG_DSI_PLL_SDM_SSC_PRD, 0x1B1);

			delta1 = (dsi->ext->params->ssc_range == 0) ?
				delta1 : dsi->ext->params->ssc_range;
			pdelta1 = (delta1 * (data_Rate / 2) * pcw_ratio *
				   262144 + 281664) / 563329;

			BDG_OUTREGBIT(cmdq, struct MIPI_TX_PLL_CON3_REG,
					MIPI_TX_REG->MIPI_TX_PLL_CON3,
					RG_DSI_PLL_SDM_SSC_DELTA, pdelta1);
			BDG_OUTREGBIT(cmdq, struct MIPI_TX_PLL_CON3_REG,
					MIPI_TX_REG->MIPI_TX_PLL_CON3,
					RG_DSI_PLL_SDM_SSC_DELTA1, pdelta1);
			DDPINFO(
				"%s, PLL config:data_rate=%d,pcw_ratio=%d, delta1=%d,pdelta1=0x%x\n",
				__func__, data_Rate, pcw_ratio, delta1, pdelta1);
		}
	}

	for (i = DSI_MODULE_BEGIN(module); i <= DSI_MODULE_END(module); i++) {
		if (data_Rate && (dsi->ext->params->bdg_ssc_disable != 1))
			BDG_OUTREGBIT(cmdq, struct MIPI_TX_PLL_CON2_REG,
					MIPI_TX_REG->MIPI_TX_PLL_CON2,
					RG_DSI_PLL_SDM_SSC_EN, 1);
		else
			BDG_OUTREGBIT(cmdq, struct MIPI_TX_PLL_CON2_REG,
					MIPI_TX_REG->MIPI_TX_PLL_CON2,
					RG_DSI_PLL_SDM_SSC_EN, 0);
	}

	for (i = DSI_MODULE_BEGIN(module); i <= DSI_MODULE_END(module); i++) {
		/* PLL EN */
		BDG_OUTREGBIT(cmdq, struct MIPI_TX_PLL_CON1_REG,
				MIPI_TX_REG->MIPI_TX_PLL_CON1,
				RG_DSI_PLL_EN, 1);

		udelay(50);
		BDG_OUTREGBIT(cmdq, struct MIPI_TX_SW_CTRL_CON4_REG,
				MIPI_TX_REG->MIPI_TX_SW_CTRL_CON4,
				MIPI_TX_SW_ANA_CK_EN, 1);
	}

	return 0;
}

int bdg_tx_phy_config(enum DISP_BDG_ENUM module,
			void *cmdq, struct mtk_dsi *dsi)
{
	int i;
	u32 ui, cycle_time;
	unsigned int hs_trail;

	ui = 1000 / tx_data_rate;
	cycle_time = 8000 / tx_data_rate;

	pr_info(
		"%s, tx_data_rate=%d, cycle_time=%d, ui=%d\n",
		__func__, tx_data_rate, cycle_time, ui);

	/* lpx >= 50ns (spec) */
	/* lpx = 60ns */
	timcon0.LPX = NS_TO_CYCLE_BDG(60, cycle_time);
	if (timcon0.LPX < 2)
		timcon0.LPX = 2;

	/* hs_prep = 40ns+4*UI ~ 85ns+6*UI (spec) */
	/* hs_prep = 64ns+5*UI */
	timcon0.HS_PRPR = NS_TO_CYCLE_BDG((64 + 5 * ui), cycle_time) + 1;

	/* hs_zero = (200+10*UI) - hs_prep */
	timcon0.HS_ZERO = NS_TO_CYCLE_BDG((200 + 10 * ui), cycle_time);
	timcon0.HS_ZERO = timcon0.HS_ZERO > timcon0.HS_PRPR ?
			timcon0.HS_ZERO - timcon0.HS_PRPR : timcon0.HS_ZERO;
	if (timcon0.HS_ZERO < 1)
		timcon0.HS_ZERO = 1;

	/* hs_trail > max(8*UI, 60ns+4*UI) (spec) */
	/* hs_trail = 80ns+4*UI */
	hs_trail = 80 + 4 * ui;
	timcon0.HS_TRAIL = (hs_trail > cycle_time) ?
				NS_TO_CYCLE_BDG(hs_trail, cycle_time) + 1 : 2;

	/* hs_exit > 100ns (spec) */
	/* hs_exit = 120ns */
	/* timcon1.DA_HS_EXIT = NS_TO_CYCLE_BDG(120, cycle_time); */
	/* hs_exit = 2*lpx */
	timcon1.DA_HS_EXIT = 2 * timcon0.LPX;

	/* ta_go = 4*lpx (spec) */
	timcon1.TA_GO = 4 * timcon0.LPX;

	/* ta_get = 5*lpx (spec) */
	timcon1.TA_GET = 5 * timcon0.LPX;

	/* ta_sure = lpx ~ 2*lpx (spec) */
	timcon1.TA_SURE = 3 * timcon0.LPX / 2;

	/* clk_hs_prep = 38ns ~ 95ns (spec) */
	/* clk_hs_prep = 80ns */
	timcon3.CLK_HS_PRPR = NS_TO_CYCLE_BDG(80, cycle_time);

	/* clk_zero + clk_hs_prep > 300ns (spec) */
	/* clk_zero = 400ns - clk_hs_prep */
	timcon2.CLK_ZERO = NS_TO_CYCLE_BDG(400, cycle_time) -
				timcon3.CLK_HS_PRPR;
	if (timcon2.CLK_ZERO < 1)
		timcon2.CLK_ZERO = 1;

	/* clk_trail > 60ns (spec) */
	/* clk_trail = 100ns */
	timcon2.CLK_TRAIL = NS_TO_CYCLE_BDG(100, cycle_time) + 1;
	if (timcon2.CLK_TRAIL < 2)
		timcon2.CLK_TRAIL = 2;

	/* clk_exit > 100ns (spec) */
	/* clk_exit = 200ns */
	/* timcon3.CLK_EXIT = NS_TO_CYCLE_BDG(200, cycle_time); */
	/* clk_exit = 2*lpx */
	timcon3.CLK_HS_EXIT = 2 * timcon0.LPX;

	/* clk_post > 60ns+52*UI (spec) */
	/* clk_post = 96ns+52*UI */
	timcon3.CLK_HS_POST = NS_TO_CYCLE_BDG((96 + 52 * ui), cycle_time);

	bg_tx_data_phy_cycle = (timcon1.DA_HS_EXIT + 1) + timcon0.LPX +
					timcon0.HS_PRPR + timcon0.HS_ZERO + 1;

	DDPINFO(
		"%s, bg_tx_data_phy_cycle=%d, LPX=%d, HS_PRPR=%d, HS_ZERO=%d, HS_TRAIL=%d, DA_HS_EXIT=%d\n",
		__func__, bg_tx_data_phy_cycle, timcon0.LPX, timcon0.HS_PRPR,
		 timcon0.HS_ZERO, timcon0.HS_TRAIL, timcon1.DA_HS_EXIT);

	DDPINFO(
		"%s, TA_GO=%d, TA_GET=%d, TA_SURE=%d, CLK_HS_PRPR=%d, CLK_ZERO=%d, CLK_TRAIL=%d, CLK_HS_EXIT=%d, CLK_HS_POST=%d\n",
		__func__, timcon1.TA_GO, timcon1.TA_GET, timcon1.TA_SURE,
		timcon3.CLK_HS_PRPR, timcon2.CLK_ZERO, timcon2.CLK_TRAIL,
		 timcon3.CLK_HS_EXIT, timcon3.CLK_HS_POST);

	for (i = DSI_MODULE_BEGIN(module); i <= DSI_MODULE_END(module); i++) {
		BDG_OUTREGBIT(cmdq, struct DSI_TX_PHY_TIMCON0_REG,
				TX_REG[i]->DSI_TX_PHY_TIMECON0, LPX,
				timcon0.LPX);
		BDG_OUTREGBIT(cmdq, struct DSI_TX_PHY_TIMCON0_REG,
				TX_REG[i]->DSI_TX_PHY_TIMECON0, HS_PRPR,
				timcon0.HS_PRPR);
		BDG_OUTREGBIT(cmdq, struct DSI_TX_PHY_TIMCON0_REG,
				TX_REG[i]->DSI_TX_PHY_TIMECON0, HS_ZERO,
				timcon0.HS_ZERO);
		BDG_OUTREGBIT(cmdq, struct DSI_TX_PHY_TIMCON0_REG,
				TX_REG[i]->DSI_TX_PHY_TIMECON0, HS_TRAIL,
				timcon0.HS_TRAIL);

		BDG_OUTREGBIT(cmdq, struct DSI_TX_PHY_TIMCON1_REG,
				TX_REG[i]->DSI_TX_PHY_TIMECON1, TA_GO,
				timcon1.TA_GO);
		BDG_OUTREGBIT(cmdq, struct DSI_TX_PHY_TIMCON1_REG,
				TX_REG[i]->DSI_TX_PHY_TIMECON1, TA_SURE,
				timcon1.TA_SURE);
		BDG_OUTREGBIT(cmdq, struct DSI_TX_PHY_TIMCON1_REG,
				TX_REG[i]->DSI_TX_PHY_TIMECON1, TA_GET,
				timcon1.TA_GET);
		BDG_OUTREGBIT(cmdq, struct DSI_TX_PHY_TIMCON1_REG,
				TX_REG[i]->DSI_TX_PHY_TIMECON1, DA_HS_EXIT,
				timcon1.DA_HS_EXIT);

		BDG_OUTREGBIT(cmdq, struct DSI_TX_PHY_TIMCON2_REG,
				TX_REG[i]->DSI_TX_PHY_TIMECON2, CLK_ZERO,
				timcon2.CLK_ZERO);
		BDG_OUTREGBIT(cmdq, struct DSI_TX_PHY_TIMCON2_REG,
				TX_REG[i]->DSI_TX_PHY_TIMECON2, CLK_TRAIL,
				timcon2.CLK_TRAIL);

		BDG_OUTREGBIT(cmdq, struct DSI_TX_PHY_TIMCON3_REG,
				TX_REG[i]->DSI_TX_PHY_TIMECON3, CLK_HS_PRPR,
				timcon3.CLK_HS_PRPR);
		BDG_OUTREGBIT(cmdq, struct DSI_TX_PHY_TIMCON3_REG,
				TX_REG[i]->DSI_TX_PHY_TIMECON3, CLK_HS_POST,
				timcon3.CLK_HS_POST);
		BDG_OUTREGBIT(cmdq, struct DSI_TX_PHY_TIMCON3_REG,
				TX_REG[i]->DSI_TX_PHY_TIMECON3, CLK_HS_EXIT,
				timcon3.CLK_HS_EXIT);

		DDPINFO("%s, PHY_TIMECON0=0x%08x,PHY_TIMECON1=0x%08x\n",
			__func__,
			mtk_spi_read((unsigned long)(&TX_REG[i]->DSI_TX_PHY_TIMECON0)),
			mtk_spi_read((unsigned long)(&TX_REG[i]->DSI_TX_PHY_TIMECON1)));
		DDPINFO("%s, PHY_TIMECON2=0x%08x,PHY_TIMECON3=0x%08x\n",
			__func__,
			mtk_spi_read((unsigned long)(&TX_REG[i]->DSI_TX_PHY_TIMECON2)),
			mtk_spi_read((unsigned long)(&TX_REG[i]->DSI_TX_PHY_TIMECON3)));
	}

	return 0;
}

int bdg_tx_txrx_ctrl(enum DISP_BDG_ENUM module,
			void *cmdq, struct mtk_dsi *dsi)
{
	int i;
	int lane_num = dsi->lanes;
	bool hstx_cklp_en = false;
	bool dis_eotp_en = dsi->ext->params->is_cphy ? true : false;
	bool ext_te_en = (dsi->mode_flags & MIPI_DSI_MODE_VIDEO) ? false : true;

	switch (lane_num) {
	case 1:
		lane_num = 0x1;
		break;
	case 2:
		lane_num = 0x3;
		break;
	case 3:
		lane_num = 0x7;
		break;
	case 4:
	default:
		lane_num = 0xF;
		break;
	}

	for (i = DSI_MODULE_BEGIN(module); i <= DSI_MODULE_END(module); i++) {
		BDG_OUTREGBIT(cmdq, struct DSI_TX_TXRX_CON_REG,
				TX_REG[i]->DSI_TX_TXRX_CON, LANE_NUM, lane_num);
		BDG_OUTREGBIT(cmdq, struct DSI_TX_TXRX_CON_REG,
				TX_REG[i]->DSI_TX_TXRX_CON, HSTX_CKLP_EN,
				hstx_cklp_en);
		BDG_OUTREGBIT(cmdq, struct DSI_TX_TXRX_CON_REG,
				TX_REG[i]->DSI_TX_TXRX_CON, DIS_EOT,
				dis_eotp_en);
		BDG_OUTREGBIT(cmdq, struct DSI_TX_TXRX_CON_REG,
				TX_REG[i]->DSI_TX_TXRX_CON, EXT_TE_EN,
				ext_te_en);
		BDG_OUTREGBIT(cmdq, struct DSI_TX_INTEN_REG,
				TX_REG[i]->DSI_TX_INTEN, BUFFER_UNDERRUN_INT_EN,
				1);
		/* fpga mode */
		//BDG_OUTREG32(cmdq, &TX_REG[i]->DSI_PHY_LCPAT, 0x55);
	}

	return 0;
}

int bdg_tx_ps_ctrl(enum DISP_BDG_ENUM module,
			void *cmdq, struct mtk_dsi *dsi)
{
	int i;
	unsigned int ps_wc, bpp, ps_sel;
	u32 width = mtk_dsi_get_virtual_width(dsi, dsi->encoder.crtc);
	u32 height = mtk_dsi_get_virtual_heigh(dsi, dsi->encoder.crtc);

	switch (dsi->format) {
	case MIPI_DSI_FMT_RGB888:
		ps_sel = PACKED_PS_24BIT_RGB888;
		bpp = 24;
		break;
	case MIPI_DSI_FMT_RGB666:
		ps_sel = PACKED_PS_18BIT_RGB666;
		bpp = 18;
		break;
	case MIPI_DSI_FMT_RGB666_PACKED:
		ps_sel = LOOSELY_PS_24BIT_RGB666;
		bpp = 24;
		break;
	case MIPI_DSI_FMT_RGB565:
		ps_sel = PACKED_PS_16BIT_RGB565;
		bpp = 16;
		break;
	default:
		DDPMSG("format not support!!!\n");
		return -1;
	}

	if (dsc_en) {
		ps_sel = PACKED_COMPRESSION;
		width = (width + 2) / 3;
	}
	ps_wc = (width * bpp) / 8;
	DDPINFO(
		"%s, DSI_WIDTH=%d, DSI_HEIGHT=%d, ps_sel=%d, ps_wc=%d\n",
		__func__, width, height, ps_sel, ps_wc);

	for (i = DSI_MODULE_BEGIN(module); i <= DSI_MODULE_END(module); i++) {
		BDG_OUTREGBIT(cmdq, struct DSI_TX_VACT_NL_REG,
				TX_REG[i]->DSI_TX_VACT_NL, VACT_NL,
				height / line_back_to_LP);

		BDG_OUTREGBIT(cmdq, struct DSI_TX_PSCON_REG,
				TX_REG[i]->DSI_TX_PSCON, CUSTOM_HEADER, 0);
		BDG_OUTREGBIT(cmdq, struct DSI_TX_PSCON_REG,
				TX_REG[i]->DSI_TX_PSCON, DSI_PS_WC, ps_wc * line_back_to_LP);
		BDG_OUTREGBIT(cmdq, struct DSI_TX_PSCON_REG,
				TX_REG[i]->DSI_TX_PSCON, DSI_PS_SEL, ps_sel);

		BDG_OUTREGBIT(cmdq, struct DSI_TX_SIZE_CON_REG,
				TX_REG[i]->DSI_TX_SIZE_CON, DSI_WIDTH,
				width * line_back_to_LP);
		BDG_OUTREGBIT(cmdq, struct DSI_TX_SIZE_CON_REG,
				TX_REG[i]->DSI_TX_SIZE_CON, DSI_HEIGHT,
				height / line_back_to_LP);
	}

	return 0;
}

int bdg_tx_vdo_timing_set(enum DISP_BDG_ENUM module,
			void *cmdq, struct mtk_dsi *dsi)
{
	int i;
	u32 dsi_buf_bpp, data_init_byte;
	struct videomode *vm = &dsi->vm;
	u32 t_vfp = vm->vfront_porch;
	u32 t_vbp = vm->vback_porch;
	u32 t_vsa = vm->vsync_len;
	u32 t_hfp = vm->hfront_porch;
	u32 t_hbp = vm->hback_porch;
	u32 t_hsa = vm->hsync_len;

	if (dsi->format == MIPI_DSI_FMT_RGB565)
		dsi_buf_bpp = 16;
	else
		dsi_buf_bpp = 24;

	if (dsi->ext->params->is_cphy) {
		DDPMSG("C-PHY mode, need check!!!\n");
	} else {
		data_init_byte = bg_tx_data_phy_cycle * dsi->lanes;

		if (dsi->mode_flags & MIPI_DSI_MODE_VIDEO) {
			if (dsi->mode_flags & MIPI_DSI_MODE_VIDEO_SYNC_PULSE) {
				hsa_byte = (((t_hsa * dsi_buf_bpp) / 8) - 10);
				hbp_byte = (((t_hbp * dsi_buf_bpp) / 8) - 10);
				hfp_byte = (((t_hfp * dsi_buf_bpp) / 8) - 12);
			} else if (dsi->mode_flags & MIPI_DSI_MODE_VIDEO_BURST) {
				hsa_byte = 0;	/* don't care */
				hbp_byte = (((t_hbp + t_hsa) * dsi_buf_bpp) / 8) - 10;
				hfp_byte = (((t_hfp * dsi_buf_bpp) / 8) - 12 - 6);
			} else {
				hsa_byte = 0;	/* don't care */
				hbp_byte = (((t_hbp + t_hsa) * dsi_buf_bpp) / 8) - 10;
				hfp_byte = (((t_hfp * dsi_buf_bpp) / 8) - 12);
			}
		}

		bllp_byte = 16 * dsi->lanes;
	}

	if (hsa_byte < 0) {
		DDPMSG("error!hsa = %d < 0!\n", hsa_byte);
		hsa_byte = 0;
	}

	if (hfp_byte > data_init_byte)
		hfp_byte -= data_init_byte;
	else {
		hfp_byte = 4;
		DDPMSG("hfp is too short!\n");
	}

	DDPINFO(
		"%s, t_vsa=%d, t_vbp=%d, t_vfp=%d, hsa_byte=%d, hbp_byte=%d, hfp_byte=%d, bllp_byte=%d\n",
		__func__, t_vsa, t_vbp, t_vfp, hsa_byte, hbp_byte, hfp_byte, bllp_byte);

	for (i = DSI_MODULE_BEGIN(module); i <= DSI_MODULE_END(module); i++) {
		BDG_OUTREG32(cmdq, TX_REG[i]->DSI_TX_VSA_NL, t_vsa);
		BDG_OUTREG32(cmdq, TX_REG[i]->DSI_TX_VBP_NL, t_vbp);
		BDG_OUTREG32(cmdq, TX_REG[i]->DSI_TX_VFP_NL, t_vfp);

		BDG_OUTREG32(cmdq, TX_REG[i]->DSI_TX_HSA_WC, hsa_byte);
		BDG_OUTREG32(cmdq, TX_REG[i]->DSI_TX_HBP_WC, hbp_byte);
		BDG_OUTREG32(cmdq, TX_REG[i]->DSI_TX_HFP_WC, hfp_byte);
		BDG_OUTREG32(cmdq, TX_REG[i]->DSI_TX_BLLP_WC, bllp_byte);
	}

	return 0;
}

int bdg_tx_buf_rw_set(enum DISP_BDG_ENUM module,
			void *cmdq, struct mtk_dsi *dsi)
{
	int i;
	unsigned int width, height, rw_times, tmp;

	tmp = (mtk_spi_read((unsigned long)
			(&TX_REG[0]->DSI_TX_COM_CON)) & 0x01000000) >> 24;
	width = mtk_dsi_get_virtual_width(dsi, dsi->encoder.crtc);
	height = mtk_dsi_get_virtual_heigh(dsi, dsi->encoder.crtc);

	if (dsc_en)
		width = (width + 2) / 3;

	if (tmp == 1 && (dsi->mode_flags & MIPI_DSI_MODE_VIDEO) == 0) {
		if ((width * 3 % 9) == 0)
			rw_times = (width * 3 / 9) * height;
		else
			rw_times = (width * 3 / 9 + 1) * height;
	} else {
		if ((width * height * 3 % 9) == 0)
			rw_times = width * height * 3 / 9;
		else
			rw_times = width * height * 3 / 9 + 1;
	}

	DDPINFO(
		"%s, mode=0x%x, tmp=%d, width=%d, height=%d, rw_times=%d\n",
		__func__, dsi->mode_flags, tmp, width, height, rw_times);

	for (i = DSI_MODULE_BEGIN(module); i <= DSI_MODULE_END(module); i++) {
		BDG_OUTREG32(cmdq, TX_REG[i]->DSI_TX_BUF_RW_TIMES, rw_times);
		BDG_OUTREGBIT(cmdq, struct DSI_TX_BUF_CON0_REG,
			TX_REG[i]->DSI_TX_BUF_CON0, ANTI_LATENCY_BUF_EN, 1);
		if ((dsi->mode_flags & MIPI_DSI_MODE_VIDEO) == 0)
			BDG_OUTREG32(cmdq, TX_REG[i]->DSI_TX_BUF_CON1, 0x0dfd0200);
	}

	return 0;
}

int bdg_tx_enable_hs_clk(enum DISP_BDG_ENUM module,
				void *cmdq, bool enable)
{
	int i;

	for (i = DSI_MODULE_BEGIN(module); i <= DSI_MODULE_END(module); i++) {
		if (enable)
			BDG_OUTREGBIT(cmdq, struct DSI_TX_PHY_LCCON_REG,
				TX_REG[i]->DSI_TX_PHY_LCCON, LC_HS_TX_EN, 1);
		else
			BDG_OUTREGBIT(cmdq, struct DSI_TX_PHY_LCCON_REG,
				TX_REG[i]->DSI_TX_PHY_LCCON, LC_HS_TX_EN, 0);
	}
	return 0;
}

#ifdef BDG_PORTING_DBG
int dsi_set_fps(lcm_dsi_params *dsi_params, enum dsi_fps_enum fps)
{
	int i;
	u32 dsi_buf_bpp, hactive;
	u32 line_time, frame_time;

	if ((fps > MTK_DSI_FPS_90) || (fps == 0))
		return -DSI_STATUS_ERROR;

	hactive = dsi_params->hactive_pixel / 1;

	dsi_buf_bpp = 24;

	line_time =
	    (hactive + dsi_params->hsync_active + dsi_params->hback_porch +
	     dsi_params->hfront_porch) * dsi_buf_bpp;
	frame_time =
	    line_time * (dsi_params->vactive_line + dsi_params->vsync_active +
			 dsi_params->vback_porch + dsi_params->vfront_porch);

	dsi_params->data_rate =
	    ((frame_time / dsi_params->lane_num) * ((u32)fps) + 999) / 1000;

	DDPDUMP("fps=%d, lanes=%d, data rate=%d.%03d MHz\n",
		fps, dsi_params->lane_num, dsi_params->data_rate / 1000,
		dsi_params->data_rate % 1000);

	return 0;
}
#endif

int bdg_tx_set_mode(enum DISP_BDG_ENUM module,
				void *cmdq, struct mtk_dsi *dsi)
{
	int i = 0;
	u32 vid_mode = CMD_MODE;

	DDPINFO("%s\n", __func__);

	if (dsi && (dsi->mode_flags & MIPI_DSI_MODE_VIDEO)) {
		if (dsi->mode_flags & MIPI_DSI_MODE_VIDEO_BURST)
			vid_mode = BURST_MODE;
		else if (dsi->mode_flags & MIPI_DSI_MODE_VIDEO_SYNC_PULSE)
			vid_mode = SYNC_PULSE_MODE;
		else
			vid_mode = SYNC_EVENT_MODE;
	}

	for (i = DSI_MODULE_BEGIN(module); i <= DSI_MODULE_END(module); i++)
		BDG_OUTREGBIT(cmdq, struct DSI_TX_MODE_CON_REG,
				TX_REG[i]->DSI_TX_MODE_CON, MODE, vid_mode);

	if (vid_mode == CMD_MODE)
		BDG_OUTREGBIT(cmdq, struct MIPI_RX_POST_CTRL_REG,
			DISPSYS_REG->MIPI_RX_POST_CTRL, MIPI_RX_MODE_SEL, 0);
	else
		BDG_OUTREGBIT(cmdq, struct MIPI_RX_POST_CTRL_REG,
			DISPSYS_REG->MIPI_RX_POST_CTRL, MIPI_RX_MODE_SEL, 1);

	return 0;
}

int bdg_tx_bist_pattern(enum DISP_BDG_ENUM module,
				void *cmdq, bool enable, unsigned int sel,
				unsigned int red, unsigned int green,
				unsigned int blue)
{
	int i;

	for (i = DSI_MODULE_BEGIN(module); i <= DSI_MODULE_END(module); i++) {
		if (enable) {
			BDG_OUTREGBIT(cmdq, struct DSI_TX_SELF_PAT_CON0_REG,
					TX_REG[i]->DSI_TX_SELF_PAT_CON0,
					SELF_PAT_R, (red & 0xff) << 4);
			BDG_OUTREGBIT(cmdq, struct DSI_TX_SELF_PAT_CON1_REG,
					TX_REG[i]->DSI_TX_SELF_PAT_CON1,
					SELF_PAT_G, (green & 0xff) << 4);
			BDG_OUTREGBIT(cmdq, struct DSI_TX_SELF_PAT_CON1_REG,
					TX_REG[i]->DSI_TX_SELF_PAT_CON1,
					SELF_PAT_B, (blue & 0xff) << 4);
			BDG_OUTREGBIT(cmdq, struct DSI_TX_SELF_PAT_CON0_REG,
					TX_REG[i]->DSI_TX_SELF_PAT_CON0,
					SELF_PAT_SEL, sel);
			BDG_OUTREGBIT(cmdq, struct DSI_TX_SELF_PAT_CON0_REG,
					TX_REG[i]->DSI_TX_SELF_PAT_CON0,
					SELF_PAT_PRE_MODE, 1);
		} else {
			BDG_OUTREGBIT(cmdq, struct DSI_TX_SELF_PAT_CON0_REG,
					TX_REG[i]->DSI_TX_SELF_PAT_CON0,
					SELF_PAT_PRE_MODE, 0);
		}

	}

	return 0;
}

int bdg_tx_start(enum DISP_BDG_ENUM module, void *cmdq)
{
	int i;

	for (i = DSI_MODULE_BEGIN(module); i <= DSI_MODULE_END(module); i++) {
		BDG_OUTREGBIT(cmdq, struct DSI_TX_START_REG,
				TX_REG[i]->DSI_TX_START, DSI_TX_START, 0);
		BDG_OUTREGBIT(cmdq, struct DSI_TX_START_REG,
				TX_REG[i]->DSI_TX_START, DSI_TX_START, 1);
	}

	return 0;
}

int bdg_set_dcs_read_cmd(bool enable, void *cmdq)
{
	if (enable) {
		BDG_OUTREGBIT(cmdq, struct DSI_TX_RACK_REG,
			TX_REG[0]->DSI_TX_RACK, DSI_TX_RACK_BYPASS, 1);
		BDG_OUTREGBIT(cmdq, struct MIPI_RX_POST_CTRL_REG,
			DISPSYS_REG->MIPI_RX_POST_CTRL, MIPI_RX_MODE_SEL, 0);
	} else {
		BDG_OUTREGBIT(cmdq, struct DSI_TX_RACK_REG,
			TX_REG[0]->DSI_TX_RACK, DSI_TX_RACK_BYPASS, 0);
		BDG_OUTREGBIT(cmdq, struct MIPI_RX_POST_CTRL_REG,
			DISPSYS_REG->MIPI_RX_POST_CTRL, MIPI_RX_MODE_SEL, 1);
	}
	return 0;
}

int bdg_tx_stop(enum DISP_BDG_ENUM module, void *cmdq)
{
	int i;

	for (i = DSI_MODULE_BEGIN(module); i <= DSI_MODULE_END(module); i++)
		BDG_OUTREGBIT(cmdq, struct DSI_TX_START_REG,
				TX_REG[i]->DSI_TX_START, DSI_TX_START, 0);

	return 0;
}

int bdg_tx_reset(enum DISP_BDG_ENUM module, void *cmdq)
{
	int i;

	for (i = DSI_MODULE_BEGIN(module); i <= DSI_MODULE_END(module); i++) {
		BDG_OUTREGBIT(cmdq, struct DSI_TX_COM_CON_REG,
				TX_REG[i]->DSI_TX_COM_CON, DSI_RESET, 1);
		BDG_OUTREGBIT(cmdq, struct DSI_TX_COM_CON_REG,
				TX_REG[i]->DSI_TX_COM_CON, DSI_RESET, 0);
	}

	return 0;
}

void bdg_rx_reset(void *cmdq)
{
	BDG_OUTREG32(cmdq, DSI2_REG->DSI2_DEVICE_SOFT_RSTN_OS, 1);
	udelay(1);
	BDG_OUTREG32(cmdq, DSI2_REG->DSI2_DEVICE_SOFT_RSTN_OS, 0);
	udelay(1);
	BDG_OUTREG32(cmdq, DSI2_REG->DSI2_DEVICE_SOFT_RSTN_OS, 1);
}

int bdg_vm_mode_set(enum DISP_BDG_ENUM module, bool enable,
			unsigned int long_pkt, void *cmdq)
{
	int i;

	for (i = DSI_MODULE_BEGIN(module); i <= DSI_MODULE_END(module); i++) {
		if (enable) {
			BDG_OUTREG32(cmdq, TX_REG[i]->DSI_VM_CMD_CON1, 0x10);
			BDG_OUTREG32(cmdq, DISPSYS_REG->DDI_POST_CTRL, 0x00001110);
			if (long_pkt > 1)
				BDG_OUTREG32(cmdq, TX_REG[i]->DSI_VM_CMD_CON0, 0x37);
			else
				BDG_OUTREG32(cmdq, TX_REG[i]->DSI_VM_CMD_CON0, 0x35);
		} else {
			BDG_OUTREG32(cmdq, TX_REG[i]->DSI_VM_CMD_CON0, 0);
			BDG_OUTREG32(cmdq, DISPSYS_REG->DDI_POST_CTRL, 0x00001100);
		}
	}
	return 0;
}

int bdg_tx_cmd_mode(enum DISP_BDG_ENUM module, void *cmdq)
{
	int i;

	for (i = DSI_MODULE_BEGIN(module); i <= DSI_MODULE_END(module); i++) {
		BDG_OUTREG32(cmdq, TX_REG[i]->DSI_TX_MEM_CONTI, 0x3c);
		BDG_OUTREG32(cmdq, TX_REG[i]->DSI_TX_CMDQ, 0x2c3909);
		BDG_OUTREG32(cmdq, TX_REG[i]->DSI_TX_CMDQ_CON, 1);
	}

	return 0;
}

int bdg_mutex_trigger(enum DISP_BDG_ENUM module, void *cmdq)
{

	BDG_OUTREGBIT(cmdq, struct DISP_MUTEX0_EN_REG,
			MUTEX_REG->DISP_MUTEX0_EN, MUTEX0_EN, 0);
	BDG_OUTREGBIT(cmdq, struct DISP_MUTEX0_EN_REG,
			MUTEX_REG->DISP_MUTEX0_EN, MUTEX0_EN, 1);
	return 0;
}

int bdg_dsi_dump_reg(enum DISP_BDG_ENUM module)
{
	unsigned int i, k;

	for (i = DSI_MODULE_BEGIN(module); i <= DSI_MODULE_END(module); i++) {
		unsigned long dsc_base_addr = DISPSYS_BDG_DISP_DSC_BASE;
		unsigned long dsi_base_addr = DISPSYS_BDG_TX_DSI0_BASE;
		unsigned long mipi_base_addr = DISPSYS_BDG_MIPI_TX_BASE;

		DDPMSG("===== mt6382 RX REGS =====\n");
		DDPMSG("0x%08x: 0x%08x\n", 0x0000d00c, mtk_spi_read(0x0000d00c));
		DDPMSG("0x%08x: 0x%08x\n", 0x0000d2d0, mtk_spi_read(0x0000d2d0));
		DDPMSG("0x%08x: 0x%08x\n", 0x0000d2b0, mtk_spi_read(0x0000d2b0));
		DDPMSG("0x%08x: 0x%08x\n", 0x0000d270, mtk_spi_read(0x0000d270));
		DDPMSG("0x%08x: 0x%08x\n", 0x0000d250, mtk_spi_read(0x0000d250));
		DDPMSG("0x%08x: 0x%08x\n", 0x0000d230, mtk_spi_read(0x0000d230));
		DDPMSG("0x%08x: 0x%08x\n", 0x0000d210, mtk_spi_read(0x0000d210));
		DDPMSG("0x%08x: 0x%08x\n", 0x0000d280, mtk_spi_read(0x0000d280));
		DDPMSG("0x%08x: 0x%08x\n", 0x0000d260, mtk_spi_read(0x0000d260));
		DDPMSG("0x%08x: 0x%08x\n", 0x0000d240, mtk_spi_read(0x0000d240));
		DDPMSG("0x%08x: 0x%08x\n", 0x0000d220, mtk_spi_read(0x0000d220));
		DDPMSG("0x%08x: 0x%08x\n", 0x0000d200, mtk_spi_read(0x0000d200));
		DDPMSG("0x%08x: 0x%08x\n", 0x0000d408, mtk_spi_read(0x0000d408));
		DDPMSG("===== mt6382 DSI%d REGS =====\n", i);

		for (k = 0; k < 0x210; k += 16) {
			DDPMSG("0x%08x: 0x%08x 0x%08x 0x%08x 0x%08x\n",
				(k + dsi_base_addr),
				mtk_spi_read(dsi_base_addr + k),
				mtk_spi_read(dsi_base_addr + k + 0x4),
				mtk_spi_read(dsi_base_addr + k + 0x8),
				mtk_spi_read(dsi_base_addr + k + 0xc));
		}

		DDPMSG(" ===== mt6382 DSI%d CMD REGS =====\n", i);
		for (k = 0; k < 32; k += 16) { /* only dump first 32 bytes cmd */
			DDPMSG("0x%08x: 0x%08x 0x%08x 0x%08x 0x%08x\n",
				(k + 0xd00 + dsi_base_addr),
				mtk_spi_read((dsi_base_addr + 0xd00 + k)),
				mtk_spi_read((dsi_base_addr + 0xd00 + k + 0x4)),
				mtk_spi_read((dsi_base_addr + 0xd00 + k + 0x8)),
				mtk_spi_read((dsi_base_addr + 0xd00 + k + 0xc)));
		}

		DDPMSG("===== mt6382 MIPI%d REGS ======\n", i);
		for (k = 0; k < 0x200; k += 16) {
			DDPMSG("0x%08x: 0x%08x 0x%08x 0x%08x 0x%08x\n",
				(k + mipi_base_addr),
				mtk_spi_read(mipi_base_addr + k),
				mtk_spi_read(mipi_base_addr + k + 0x4),
				mtk_spi_read(mipi_base_addr + k + 0x8),
				mtk_spi_read(mipi_base_addr + k + 0xc));
		}

		if (dsc_en) {
			DDPMSG("====== mt6382 DSC%d REGS ======\n", i);
			for (k = 0; k < sizeof(struct BDG_DISP_DSC_REGS); k += 16) {
				DDPMSG("0x%08x: 0x%08x 0x%08x 0x%08x 0x%08x\n",
					(k + dsc_base_addr),
					mtk_spi_read(dsc_base_addr + k),
					mtk_spi_read(dsc_base_addr + k + 0x4),
					mtk_spi_read(dsc_base_addr + k + 0x8),
					mtk_spi_read(dsc_base_addr + k + 0xc));
			}
		}
	}

	return 0;
}

int bdg_tx_wait_for_idle(enum DISP_BDG_ENUM module)
{
	int i;
	unsigned int timeout = 5000; /* unit: usec */
	unsigned int status;

	for (i = DSI_MODULE_BEGIN(module); i <= DSI_MODULE_END(module); i++) {
		while (timeout) {
			udelay(1);
			status = mtk_spi_read((unsigned long)(&TX_REG[i]->DSI_TX_INTSTA));
//			DDPMSG("%s, i=%d, status=0x%x, timeout=%d\n",
//				__func__, i, status, timeout);

			if (!(status & 0x80000000))
				break;
			timeout--;
		}
	}
	if (timeout == 0) {
//		dsi_dump_reg(module, 0);
		DDPMSG("%s, wait timeout!\n", __func__);
		return -1;
	}

	return 0;
}

int bdg_dsi_line_timing_dphy_setting(enum DISP_BDG_ENUM module,
			void *cmdq, struct mtk_dsi *dsi)
{
	unsigned int width, height, lanes, ps_wc, new_hfp_byte;
	unsigned int bg_tx_total_word_cnt = 0;
	unsigned int bg_tx_line_time = 0, disp_pipe_line_time = 0;
	unsigned int rxtx_ratio = 0;

	width = mtk_dsi_get_virtual_width(dsi, dsi->encoder.crtc);
	height = mtk_dsi_get_virtual_heigh(dsi, dsi->encoder.crtc);
	lanes = dsi->lanes;

	/* Step 1. Show Bridge DSI TX Setting. */
	/* get bdg-tx hsa_byte, hbp_byte, new_hfp_byte and bllp_byte */
	if (dsc_en) {
		ps_wc = width * 24 / 8 / 3;	/* for 8bpp, 1/3 compression */
		rxtx_ratio = bdg_rxtx_ratio;	/* ratio=2.25 */
	} else {
		ps_wc = width * 24 / 8;	/* for 8bpp, 1/3 compression */
		rxtx_ratio = 100;
	}
	new_hfp_byte = hfp_byte;

	DDPMSG("%s, dsc_en=%d, hsa_byte=%d, hbp_byte=%d\n",
		__func__, dsc_en, hsa_byte, hbp_byte);
	DDPMSG("%s, new_hfp_byte=%d, bllp_byte=%d, ps_wc=%d\n",
		__func__, new_hfp_byte, bllp_byte, ps_wc);

	/* get lpx, hs_prep, hs_zero,... refer to bdg_tx_phy_config()*/

	/* Step 2. Bridge DSI TX Cycle Count. */
	/* get bg_tx_line_cycle, bg_tx_total_word_cnt */
	/* D-PHY, DSI TX (Bridge IC) back to LP mode during V-Active */

	if (dsi->mode_flags & MIPI_DSI_MODE_VIDEO) {
		if (dsi->mode_flags & MIPI_DSI_MODE_VIDEO_BURST)
			bg_tx_total_word_cnt = 4 +	/* hss packet */
			(4 + hbp_byte + 2) +	/* hbp packet */
			(4 + ps_wc + 2) +	/* rgb packet */
			(4 + bllp_byte + 2) +	/* bllp packet*/
			(4 + new_hfp_byte + 2) +/* hfp packet */
			bg_tx_data_phy_cycle * lanes;
		else if (dsi->mode_flags & MIPI_DSI_MODE_VIDEO_SYNC_PULSE)
			bg_tx_total_word_cnt = 4 +	/* hss packet */
			(4 + hsa_byte + 2) +	/* hsa packet */
			4 +			/* hse packet */
			(4 + hbp_byte + 2) +	/* hbp packet */
			(4 + ps_wc + 2) +	/* rgb packet */
			(4 + new_hfp_byte + 2) +/* hfp packet */
			bg_tx_data_phy_cycle * lanes;
		else
			bg_tx_total_word_cnt = 4 +	/* hss packet */
			(4 + hbp_byte + 2) +	/* hbp packet */
			(4 + ps_wc + 2) +	/* rgb packet */
			(4 + new_hfp_byte + 2) +/* hfp packet */
			bg_tx_data_phy_cycle * lanes;
	} else
		bg_tx_total_word_cnt = 0;

	bg_tx_line_cycle = (bg_tx_total_word_cnt + (lanes - 1)) / lanes;
	bg_tx_line_time = bg_tx_line_cycle * 8000 / tx_data_rate;

	disp_pipe_line_time = width * 1000 / MM_CLK;

	DDPMSG("bg_tx_total_word_cnt=%d, bg_tx_line_cycle=%d\n",
		bg_tx_total_word_cnt, bg_tx_line_cycle);
	DDPMSG("disp_pipe_line_time=%d, bg_tx_line_time=%d\n",
		disp_pipe_line_time, bg_tx_line_time);

	if ((dsi->mode_flags & MIPI_DSI_MODE_VIDEO) && (disp_pipe_line_time > bg_tx_line_time)) {
		DDPMSG("error!! disp_pipe_line_time(%d) > bg_tx_line_time(%d)\n",
			disp_pipe_line_time, bg_tx_line_time);

		return -1;
	}
	/* Step 3. Decide AP DSI TX Data Rate and PHY Timing */
	/* get ap_tx_data_rate and set back to ap */
	/* get lpx, hs_prep, hs_zero,... refer to DSI_DPHY_TIMCONFIG()*/
	ap_tx_data_rate = tx_data_rate * rxtx_ratio / 100;

	/* Step 4. Decide AP DSI TX Blanking */
	/* get ap-tx hsa_byte, hbp_byte, new_hfp_byte and bllp_byte */
	/* refer to DSI_DPHY_Calc_VDO_Timing() */

	/* Step 5. fine-tune data rate */

#ifdef aaa
	if (dsc_en) {
		ap_tx_hsa_wc = 4;
		ap_tx_hbp_wc = 4;
	} else {
		ap_tx_hsa_wc = hsa_byte;
		ap_tx_hbp_wc = hbp_byte;
		ap_tx_data_phy_cycle = bg_tx_data_phy_cycle;
	}
	ap_tx_bllp_wc  = bllp_byte;
	ap_tx_total_word_cnt = bg_tx_line_cycle * lanes * rxtx_ratio / 100;

	switch (tx_params->mode) {
	case CMD_MODE:
		ap_tx_total_word_cnt_no_hfp_wc = 0;
		break;
	case SYNC_PULSE_VDO_MODE:
		ap_tx_total_word_cnt_no_hfp_wc = 4 +	/* hss packet */
			(4 + ap_tx_hsa_wc + 2) +	/* hsa packet */
			4 +				/* hse packet */
			(4 + ap_tx_hbp_wc + 2) +	/* hbp packet */
			(4 + ps_wc + 2) +		/* rgb packet */
			(4 + 2) +			/* hfp packet */
			ap_tx_data_phy_cycle * lanes;
		break;
	case SYNC_EVENT_VDO_MODE:
		ap_tx_total_word_cnt_no_hfp_wc = 4 +	/* hss packet */
			(4 + ap_tx_hbp_wc + 2) +	/* hbp packet */
			(4 + ps_wc + 2) +	/* rgb packet */
			(4 + 2) +/* hfp packet */
			ap_tx_data_phy_cycle * lanes;
		break;
	case BURST_VDO_MODE:
		ap_tx_total_word_cnt_no_hfp_wc = 4 +	/* hss packet */
			(4 + ap_tx_hbp_wc + 2) +	/* hbp packet */
			(4 + ps_wc + 2) +	/* rgb packet */
			(4 + ap_tx_bllp_wc + 2) +	/* bllp packet*/
			(4 + 2) +/* hfp packet */
			ap_tx_data_phy_cycle * lanes;
		break;
	}

	ap_tx_hfp_wc = ap_tx_total_word_cnt - ap_tx_total_word_cnt_no_hfp_wc;

	DDPMSG(
		"%s, ap_tx_hsa_wc=%d, ap_tx_hbp_wc=%d, ap_tx_bllp_wc=%d, ap_tx_data_phy_cycle=%d\n",
		__func__, ap_tx_hsa_wc, ap_tx_hbp_wc, ap_tx_bllp_wc, ap_tx_data_phy_cycle);
	DDPMSG(
		"%s, ap_tx_hfp_wc=%d, ap_tx_total_word_cnt=%d, ap_tx_total_word_cnt_no_hfp_wc=%d\n",
		__func__, ap_tx_hfp_wc, ap_tx_total_word_cnt, ap_tx_total_word_cnt_no_hfp_wc);

#endif
	return 0;
}

unsigned int get_ap_data_rate(void)
{
	DDPMSG("%s, ap_tx_data_rate=%d\n", __func__, ap_tx_data_rate);

	return ap_tx_data_rate;
}

unsigned int get_bdg_data_rate(void)
{
	DDPMSG("%s, tx_data_rate=%d\n", __func__, tx_data_rate);

	return tx_data_rate;
}

int set_bdg_data_rate(unsigned int data_rate)
{

	if (data_rate > 2500 || data_rate < 100) {
		DDPINFO("error, please check data rate=%d MHz\n", data_rate);
		return -1;
	}
	tx_data_rate = data_rate;

	return 0;
}

unsigned int get_bdg_line_cycle(void)
{
	DDPMSG("%s, bg_tx_line_cycle=%d\n", __func__, bg_tx_line_cycle);

	return bg_tx_line_cycle;
}

unsigned int get_dsc_state(void)
{
	DDPMSG("%s, dsc_en=%d\n", __func__, dsc_en);

	return dsc_en;
}

unsigned int get_mt6382_init(void)
{
	DDPMSG("%s, mt6382_init=%d\n", __func__, mt6382_init);
	return mt6382_init;
}

unsigned int get_bdg_tx_mode(void)
{
	DDPMSG("%s, bdg_tx_mode=%d\n", __func__, bdg_tx_mode);

	return bdg_tx_mode;
}

#define DELAY_US 1
void mt6382_nt36672c_fhd_vdo_init(bool dsc_on)
{

	//lcm_dcs_write_seq_static(ctx, 0xFF, 0X10);
	mtk_spi_write(0x00021d00, 0x10ff2300);
	mtk_spi_write(0x00021060, 0x00000001); //DSI_CMDQ_CON
	mtk_spi_write(0x00021000, 0x00000000); //DSI_START
	mtk_spi_write(0x00021000, 0x00000001); //DSI_START
	while ((mtk_spi_read(0x0002100c) & 0x2) != 0x2) { //wait dsi is not busy
		DDPMSG("%s, status=0x%x\n",
			__func__, mtk_spi_read((unsigned long)(&TX_REG[0]->DSI_TX_INTSTA)));
		udelay(DELAY_US);
	}
	mtk_spi_write(0x0002100c, 0xfffd); //write 0 clear
	udelay(0);

	//lcm_dcs_write_seq_static(ctx, 0xFB, 0x01);
	mtk_spi_write(0x00021d00, 0x01fb2300);
	mtk_spi_write(0x00021060, 0x00000001); //DSI_CMDQ_CON
	mtk_spi_write(0x00021000, 0x00000000); //DSI_START
	mtk_spi_write(0x00021000, 0x00000001); //DSI_START
	while ((mtk_spi_read(0x0002100c) & 0x2) != 0x2) { //wait dsi is not busy
		DDPMSG("%s, status=0x%x\n",
			__func__, mtk_spi_read((unsigned long)(&TX_REG[0]->DSI_TX_INTSTA)));
		udelay(DELAY_US);
	}
	mtk_spi_write(0x0002100c, 0xfffd); //write 0 clear
	udelay(0);

	DDPMSG("%s, dsc_en=%d, tx_data_rate=%d\n", __func__, dsc_on, tx_data_rate);
	//DSC ON && set PPS
	if (1) {
		//lcm_dcs_write_seq_static(ctx, 0xC0, 0x03);
		mtk_spi_write(0x00021d00, 0x03c02300);
		mtk_spi_write(0x00021060, 0x00000001); //DSI_CMDQ_CON
		mtk_spi_write(0x00021000, 0x00000000); //DSI_START
		mtk_spi_write(0x00021000, 0x00000001); //DSI_START
		while ((mtk_spi_read(0x0002100c) & 0x2) != 0x2) { //wait dsi is not busy
			DDPMSG("%s, status=0x%x\n",
				__func__, mtk_spi_read((unsigned long)(&TX_REG[0]->DSI_TX_INTSTA)));
			udelay(DELAY_US);
		}
		mtk_spi_write(0x0002100c, 0xfffd); //write 0 clear
		udelay(0);
	} else {
		//lcm_dcs_write_seq_static(ctx, 0xC0, 0x00);
		mtk_spi_write(0x00021d00, 0x00c02300);
		mtk_spi_write(0x00021060, 0x00000001); //DSI_CMDQ_CON
		mtk_spi_write(0x00021000, 0x00000000); //DSI_START
		mtk_spi_write(0x00021000, 0x00000001); //DSI_START
		while ((mtk_spi_read(0x0002100c) & 0x2) != 0x2) { //wait dsi is not busy
			udelay(DELAY_US);
		}
		mtk_spi_write(0x0002100c, 0xfffd); //write 0 clear
		udelay(0);
	}

	//lcm_dcs_write_seq_static(ctx, 0xC1, 0x89, 0x28, 0x00, 0x08, 0x00, 0xAA,
	//0x02, 0x0E, 0x00, 0x2B, 0x00, 0x07, 0x0D, 0xB7,
	//0x0C, 0xB7);
	mtk_spi_write(0x00021d00, 0x00112902);
	mtk_spi_write(0x00021d04, 0x002889c1);
	mtk_spi_write(0x00021d08, 0x02aa0008);
	mtk_spi_write(0x00021d0c, 0x002b000e);
	mtk_spi_write(0x00021d10, 0x0cb70d07);
	mtk_spi_write(0x00021d14, 0x000000b7);
	mtk_spi_write(0x00021060, 0x00000006); //DSI_CMDQ_CON
	mtk_spi_write(0x00021000, 0x00000000); //DSI_START
	mtk_spi_write(0x00021000, 0x00000001); //DSI_START
	while ((mtk_spi_read(0x0002100c) & 0x2) != 0x2) { //wait dsi is not busy
		DDPMSG("%s, status=0x%x\n",
			__func__, mtk_spi_read((unsigned long)(&TX_REG[0]->DSI_TX_INTSTA)));
		udelay(DELAY_US);
	}
	mtk_spi_write(0x0002100c, 0xfffd); //write 0 clear
	udelay(0);

	//lcm_dcs_write_seq_static(ctx, 0xC2, 0x1B, 0XA0);
	mtk_spi_write(0x00021d00, 0x00032902);
	mtk_spi_write(0x00021d04, 0x00a01bc2);
	mtk_spi_write(0x00021060, 0x00000002); //DSI_CMDQ_CON
	mtk_spi_write(0x00021000, 0x00000000); //DSI_START
	mtk_spi_write(0x00021000, 0x00000001); //DSI_START
	while ((mtk_spi_read(0x0002100c) & 0x2) != 0x2) { //wait dsi is not busy
		udelay(DELAY_US);
	}
	mtk_spi_write(0x0002100c, 0xfffd); //write 0 clear
	udelay(0);

	//lcm_dcs_write_seq_static(ctx, 0xFF, 0X20);
	mtk_spi_write(0x00021d00, 0x20ff2300);
	mtk_spi_write(0x00021060, 0x00000001); //DSI_CMDQ_CON
	mtk_spi_write(0x00021000, 0x00000000); //DSI_START
	mtk_spi_write(0x00021000, 0x00000001); //DSI_START
	while ((mtk_spi_read(0x0002100c) & 0x2) != 0x2) { //wait dsi is not busy
		udelay(DELAY_US);
	}
	mtk_spi_write(0x0002100c, 0xfffd); //write 0 clear
	udelay(0);

	//lcm_dcs_write_seq_static(ctx, 0xFB, 0x01);
	mtk_spi_write(0x00021d00, 0x01fb2300);
	mtk_spi_write(0x00021060, 0x00000001); //DSI_CMDQ_CON
	mtk_spi_write(0x00021000, 0x00000000); //DSI_START
	mtk_spi_write(0x00021000, 0x00000001); //DSI_START
	while ((mtk_spi_read(0x0002100c) & 0x2) != 0x2) { //wait dsi is not busy
		udelay(DELAY_US);
	}
	mtk_spi_write(0x0002100c, 0xfffd); //write 0 clear
	udelay(0);

	//lcm_dcs_write_seq_static(ctx, 0X01, 0X66);
	mtk_spi_write(0x00021d00, 0x66011500);
	mtk_spi_write(0x00021060, 0x00000001); //DSI_CMDQ_CON
	mtk_spi_write(0x00021000, 0x00000000); //DSI_START
	mtk_spi_write(0x00021000, 0x00000001); //DSI_START
	while ((mtk_spi_read(0x0002100c) & 0x2) != 0x2) { //wait dsi is not busy
		udelay(DELAY_US);
	}
	mtk_spi_write(0x0002100c, 0xfffd); //write 0 clear
	udelay(0);

	//lcm_dcs_write_seq_static(ctx, 0X32, 0X4D);
	mtk_spi_write(0x00021d00, 0x4d321500);
	mtk_spi_write(0x00021060, 0x00000001); //DSI_CMDQ_CON
	mtk_spi_write(0x00021000, 0x00000000); //DSI_START
	mtk_spi_write(0x00021000, 0x00000001); //DSI_START
	while ((mtk_spi_read(0x0002100c) & 0x2) != 0x2) { //wait dsi is not busy
		udelay(DELAY_US);
	}
	mtk_spi_write(0x0002100c, 0xfffd); //write 0 clear
	udelay(0);

	//lcm_dcs_write_seq_static(ctx, 0X69, 0XD1);
	mtk_spi_write(0x00021d00, 0xd1691500);
	mtk_spi_write(0x00021060, 0x00000001); //DSI_CMDQ_CON
	mtk_spi_write(0x00021000, 0x00000000); //DSI_START
	mtk_spi_write(0x00021000, 0x00000001); //DSI_START
	while ((mtk_spi_read(0x0002100c) & 0x2) != 0x2) { //wait dsi is not busy
		udelay(DELAY_US);
	}
	mtk_spi_write(0x0002100c, 0xfffd); //write 0 clear
	udelay(0);

	//lcm_dcs_write_seq_static(ctx, 0XF2, 0X64);
	mtk_spi_write(0x00021d00, 0x64f22300);
	mtk_spi_write(0x00021060, 0x00000001); //DSI_CMDQ_CON
	mtk_spi_write(0x00021000, 0x00000000); //DSI_START
	mtk_spi_write(0x00021000, 0x00000001); //DSI_START
	while ((mtk_spi_read(0x0002100c) & 0x2) != 0x2) { //wait dsi is not busy
		udelay(DELAY_US);
	}
	mtk_spi_write(0x0002100c, 0xfffd); //write 0 clear
	udelay(0);

	//lcm_dcs_write_seq_static(ctx, 0XF4, 0X64);
	mtk_spi_write(0x00021d00, 0x64f42300);
	mtk_spi_write(0x00021060, 0x00000001); //DSI_CMDQ_CON
	mtk_spi_write(0x00021000, 0x00000000); //DSI_START
	mtk_spi_write(0x00021000, 0x00000001); //DSI_START
	while ((mtk_spi_read(0x0002100c) & 0x2) != 0x2) { //wait dsi is not busy
		udelay(DELAY_US);
	}
	mtk_spi_write(0x0002100c, 0xfffd); //write 0 clear
	udelay(0);

	//lcm_dcs_write_seq_static(ctx, 0XF6, 0X64);
	mtk_spi_write(0x00021d00, 0x64f62300);
	mtk_spi_write(0x00021060, 0x00000001); //DSI_CMDQ_CON
	mtk_spi_write(0x00021000, 0x00000000); //DSI_START
	mtk_spi_write(0x00021000, 0x00000001); //DSI_START
	while ((mtk_spi_read(0x0002100c) & 0x2) != 0x2) { //wait dsi is not busy
		udelay(DELAY_US);
	}
	mtk_spi_write(0x0002100c, 0xfffd); //write 0 clear
	udelay(0);

	//lcm_dcs_write_seq_static(ctx, 0XF9, 0X64);
	mtk_spi_write(0x00021d00, 0x64f92300);
	mtk_spi_write(0x00021060, 0x00000001); //DSI_CMDQ_CON
	mtk_spi_write(0x00021000, 0x00000000); //DSI_START
	mtk_spi_write(0x00021000, 0x00000001); //DSI_START
	while ((mtk_spi_read(0x0002100c) & 0x2) != 0x2) { //wait dsi is not busy
		udelay(DELAY_US);
	}
	mtk_spi_write(0x0002100c, 0xfffd); //write 0 clear
	udelay(0);

	//lcm_dcs_write_seq_static(ctx, 0XFF, 0X26);
	mtk_spi_write(0x00021d00, 0x26ff2300);
	mtk_spi_write(0x00021060, 0x00000001); //DSI_CMDQ_CON
	mtk_spi_write(0x00021000, 0x00000000); //DSI_START
	mtk_spi_write(0x00021000, 0x00000001); //DSI_START
	while ((mtk_spi_read(0x0002100c) & 0x2) != 0x2) { //wait dsi is not busy
		udelay(DELAY_US);
	}
	mtk_spi_write(0x0002100c, 0xfffd); //write 0 clear
	udelay(0);

	//lcm_dcs_write_seq_static(ctx, 0XFB, 0X01);
	mtk_spi_write(0x00021d00, 0x01fb2300);
	mtk_spi_write(0x00021060, 0x00000001); //DSI_CMDQ_CON
	mtk_spi_write(0x00021000, 0x00000000); //DSI_START
	mtk_spi_write(0x00021000, 0x00000001); //DSI_START
	while ((mtk_spi_read(0x0002100c) & 0x2) != 0x2) { //wait dsi is not busy
		udelay(DELAY_US);
	}
	mtk_spi_write(0x0002100c, 0xfffd); //write 0 clear
	udelay(0);

	//lcm_dcs_write_seq_static(ctx, 0X81, 0X0E);
	mtk_spi_write(0x00021d00, 0x0e811500);
	mtk_spi_write(0x00021060, 0x00000001); //DSI_CMDQ_CON
	mtk_spi_write(0x00021000, 0x00000000); //DSI_START
	mtk_spi_write(0x00021000, 0x00000001); //DSI_START
	while ((mtk_spi_read(0x0002100c) & 0x2) != 0x2) { //wait dsi is not busy
		udelay(DELAY_US);
	}
	mtk_spi_write(0x0002100c, 0xfffd); //write 0 clear
	udelay(0);

	//lcm_dcs_write_seq_static(ctx, 0X84, 0X03);
	mtk_spi_write(0x00021d00, 0x03841500);
	mtk_spi_write(0x00021060, 0x00000001); //DSI_CMDQ_CON
	mtk_spi_write(0x00021000, 0x00000000); //DSI_START
	mtk_spi_write(0x00021000, 0x00000001); //DSI_START
	while ((mtk_spi_read(0x0002100c) & 0x2) != 0x2) { //wait dsi is not busy
		udelay(DELAY_US);
	}
	mtk_spi_write(0x0002100c, 0xfffd); //write 0 clear
	udelay(0);

	//lcm_dcs_write_seq_static(ctx, 0X86, 0X03);
	mtk_spi_write(0x00021d00, 0x03861500);
	mtk_spi_write(0x00021060, 0x00000001); //DSI_CMDQ_CON
	mtk_spi_write(0x00021000, 0x00000000); //DSI_START
	mtk_spi_write(0x00021000, 0x00000001); //DSI_START
	while ((mtk_spi_read(0x0002100c) & 0x2) != 0x2) { //wait dsi is not busy
		udelay(DELAY_US);
	}
	mtk_spi_write(0x0002100c, 0xfffd); //write 0 clear
	udelay(0);

	//lcm_dcs_write_seq_static(ctx, 0X88, 0X07);
	mtk_spi_write(0x00021d00, 0x07881500);
	mtk_spi_write(0x00021060, 0x00000001); //DSI_CMDQ_CON
	mtk_spi_write(0x00021000, 0x00000000); //DSI_START
	mtk_spi_write(0x00021000, 0x00000001); //DSI_START
	while ((mtk_spi_read(0x0002100c) & 0x2) != 0x2) { //wait dsi is not busy
		udelay(DELAY_US);
	}
	mtk_spi_write(0x0002100c, 0xfffd); //write 0 clear
	udelay(0);

	//lcm_dcs_write_seq_static(ctx, 0XFF, 0X27);
	mtk_spi_write(0x00021d00, 0x27ff2300);
	mtk_spi_write(0x00021060, 0x00000001); //DSI_CMDQ_CON
	mtk_spi_write(0x00021000, 0x00000000); //DSI_START
	mtk_spi_write(0x00021000, 0x00000001); //DSI_START
	while ((mtk_spi_read(0x0002100c) & 0x2) != 0x2) { //wait dsi is not busy
		udelay(DELAY_US);
	}
	mtk_spi_write(0x0002100c, 0xfffd); //write 0 clear
	udelay(0);

	//lcm_dcs_write_seq_static(ctx, 0XFB, 0X01);
	mtk_spi_write(0x00021d00, 0x01fb2300);
	mtk_spi_write(0x00021060, 0x00000001); //DSI_CMDQ_CON
	mtk_spi_write(0x00021000, 0x00000000); //DSI_START
	mtk_spi_write(0x00021000, 0x00000001); //DSI_START
	while ((mtk_spi_read(0x0002100c) & 0x2) != 0x2) { //wait dsi is not busy
		udelay(DELAY_US);
	}
	mtk_spi_write(0x0002100c, 0xfffd); //write 0 clear
	udelay(0);

	//lcm_dcs_write_seq_static(ctx, 0XE3, 0X01);
	mtk_spi_write(0x00021d00, 0x01e32300);
	mtk_spi_write(0x00021060, 0x00000001); //DSI_CMDQ_CON
	mtk_spi_write(0x00021000, 0x00000000); //DSI_START
	mtk_spi_write(0x00021000, 0x00000001); //DSI_START
	while ((mtk_spi_read(0x0002100c) & 0x2) != 0x2) { //wait dsi is not busy
		udelay(DELAY_US);
	}
	mtk_spi_write(0x0002100c, 0xfffd); //write 0 clear
	udelay(0);

	//lcm_dcs_write_seq_static(ctx, 0XE4, 0XEC);
	mtk_spi_write(0x00021d00, 0xece42300);
	mtk_spi_write(0x00021060, 0x00000001); //DSI_CMDQ_CON
	mtk_spi_write(0x00021000, 0x00000000); //DSI_START
	mtk_spi_write(0x00021000, 0x00000001); //DSI_START
	while ((mtk_spi_read(0x0002100c) & 0x2) != 0x2) { //wait dsi is not busy
		udelay(DELAY_US);
	}
	mtk_spi_write(0x0002100c, 0xfffd); //write 0 clear
	udelay(0);

	//lcm_dcs_write_seq_static(ctx, 0XE5, 0X02);
	mtk_spi_write(0x00021d00, 0x02e52300);
	mtk_spi_write(0x00021060, 0x00000001); //DSI_CMDQ_CON
	mtk_spi_write(0x00021000, 0x00000000); //DSI_START
	mtk_spi_write(0x00021000, 0x00000001); //DSI_START
	while ((mtk_spi_read(0x0002100c) & 0x2) != 0x2) { //wait dsi is not busy
		udelay(DELAY_US);
	}
	mtk_spi_write(0x0002100c, 0xfffd); //write 0 clear
	udelay(0);

	//lcm_dcs_write_seq_static(ctx, 0XE6, 0XE3);
	mtk_spi_write(0x00021d00, 0xe3e62300);
	mtk_spi_write(0x00021060, 0x00000001); //DSI_CMDQ_CON
	mtk_spi_write(0x00021000, 0x00000000); //DSI_START
	mtk_spi_write(0x00021000, 0x00000001); //DSI_START
	while ((mtk_spi_read(0x0002100c) & 0x2) != 0x2) { //wait dsi is not busy
		udelay(DELAY_US);
	}
	mtk_spi_write(0x0002100c, 0xfffd); //write 0 clear
	udelay(0);

	//lcm_dcs_write_seq_static(ctx, 0XE7, 0X01);
	mtk_spi_write(0x00021d00, 0x01e72300);
	mtk_spi_write(0x00021060, 0x00000001); //DSI_CMDQ_CON
	mtk_spi_write(0x00021000, 0x00000000); //DSI_START
	mtk_spi_write(0x00021000, 0x00000001); //DSI_START
	while ((mtk_spi_read(0x0002100c) & 0x2) != 0x2) { //wait dsi is not busy
		udelay(DELAY_US);
	}
	mtk_spi_write(0x0002100c, 0xfffd); //write 0 clear
	udelay(0);

	//lcm_dcs_write_seq_static(ctx, 0XE8, 0XEC);
	mtk_spi_write(0x00021d00, 0xece82300);
	mtk_spi_write(0x00021060, 0x00000001); //DSI_CMDQ_CON
	mtk_spi_write(0x00021000, 0x00000000); //DSI_START
	mtk_spi_write(0x00021000, 0x00000001); //DSI_START
	while ((mtk_spi_read(0x0002100c) & 0x2) != 0x2) { //wait dsi is not busy
		udelay(DELAY_US);
	}
	mtk_spi_write(0x0002100c, 0xfffd); //write 0 clear
	udelay(0);

	//lcm_dcs_write_seq_static(ctx, 0XE9, 0X02);
	mtk_spi_write(0x00021d00, 0x02e92300);
	mtk_spi_write(0x00021060, 0x00000001); //DSI_CMDQ_CON
	mtk_spi_write(0x00021000, 0x00000000); //DSI_START
	mtk_spi_write(0x00021000, 0x00000001); //DSI_START
	while ((mtk_spi_read(0x0002100c) & 0x2) != 0x2) { //wait dsi is not busy
		udelay(DELAY_US);
	}
	mtk_spi_write(0x0002100c, 0xfffd); //write 0 clear
	udelay(0);

	//lcm_dcs_write_seq_static(ctx, 0XEA, 0X22);
	mtk_spi_write(0x00021d00, 0x22ea2300);
	mtk_spi_write(0x00021060, 0x00000001); //DSI_CMDQ_CON
	mtk_spi_write(0x00021000, 0x00000000); //DSI_START
	mtk_spi_write(0x00021000, 0x00000001); //DSI_START
	while ((mtk_spi_read(0x0002100c) & 0x2) != 0x2) { //wait dsi is not busy
		udelay(DELAY_US);
	}
	mtk_spi_write(0x0002100c, 0xfffd); //write 0 clear
	udelay(0);

	//lcm_dcs_write_seq_static(ctx, 0XEB, 0X03);
	mtk_spi_write(0x00021d00, 0x03eb2300);
	mtk_spi_write(0x00021060, 0x00000001); //DSI_CMDQ_CON
	mtk_spi_write(0x00021000, 0x00000000); //DSI_START
	mtk_spi_write(0x00021000, 0x00000001); //DSI_START
	while ((mtk_spi_read(0x0002100c) & 0x2) != 0x2) { //wait dsi is not busy
		udelay(DELAY_US);
	}
	mtk_spi_write(0x0002100c, 0xfffd); //write 0 clear
	udelay(0);

	//lcm_dcs_write_seq_static(ctx, 0XEC, 0X32);
	mtk_spi_write(0x00021d00, 0x32ec2300);
	mtk_spi_write(0x00021060, 0x00000001); //DSI_CMDQ_CON
	mtk_spi_write(0x00021000, 0x00000000); //DSI_START
	mtk_spi_write(0x00021000, 0x00000001); //DSI_START
	while ((mtk_spi_read(0x0002100c) & 0x2) != 0x2) { //wait dsi is not busy
		udelay(DELAY_US);
	}
	mtk_spi_write(0x0002100c, 0xfffd); //write 0 clear
	udelay(0);

	//lcm_dcs_write_seq_static(ctx, 0XED, 0X02);
	mtk_spi_write(0x00021d00, 0x02ed2300);
	mtk_spi_write(0x00021060, 0x00000001); //DSI_CMDQ_CON
	mtk_spi_write(0x00021000, 0x00000000); //DSI_START
	mtk_spi_write(0x00021000, 0x00000001); //DSI_START
	while ((mtk_spi_read(0x0002100c) & 0x2) != 0x2) { //wait dsi is not busy
		udelay(DELAY_US);
	}
	mtk_spi_write(0x0002100c, 0xfffd); //write 0 clear
	udelay(0);

	//lcm_dcs_write_seq_static(ctx, 0XEE, 0X22);
	mtk_spi_write(0x00021d00, 0x22ee2300);
	mtk_spi_write(0x00021060, 0x00000001); //DSI_CMDQ_CON
	mtk_spi_write(0x00021000, 0x00000000); //DSI_START
	mtk_spi_write(0x00021000, 0x00000001); //DSI_START
	while ((mtk_spi_read(0x0002100c) & 0x2) != 0x2) { //wait dsi is not busy
		udelay(DELAY_US);
	}
	mtk_spi_write(0x0002100c, 0xfffd); //write 0 clear
	udelay(0);

	//lcm_dcs_write_seq_static(ctx, 0XFF, 0X2A);
	mtk_spi_write(0x00021d00, 0x2aff2300);
	mtk_spi_write(0x00021060, 0x00000001); //DSI_CMDQ_CON
	mtk_spi_write(0x00021000, 0x00000000); //DSI_START
	mtk_spi_write(0x00021000, 0x00000001); //DSI_START
	while ((mtk_spi_read(0x0002100c) & 0x2) != 0x2) { //wait dsi is not busy
		udelay(DELAY_US);
	}
	mtk_spi_write(0x0002100c, 0xfffd); //write 0 clear
	udelay(0);

	//lcm_dcs_write_seq_static(ctx, 0XFB, 0X01);
	mtk_spi_write(0x00021d00, 0x01fb2300);
	mtk_spi_write(0x00021060, 0x00000001); //DSI_CMDQ_CON
	mtk_spi_write(0x00021000, 0x00000000); //DSI_START
	mtk_spi_write(0x00021000, 0x00000001); //DSI_START
	while ((mtk_spi_read(0x0002100c) & 0x2) != 0x2) { //wait dsi is not busy
		udelay(DELAY_US);
	}
	mtk_spi_write(0x0002100c, 0xfffd); //write 0 clear
	udelay(0);

	//lcm_dcs_write_seq_static(ctx, 0X0C, 0X04);
	mtk_spi_write(0x00021d00, 0x040c1500);
	mtk_spi_write(0x00021060, 0x00000001); //DSI_CMDQ_CON
	mtk_spi_write(0x00021000, 0x00000000); //DSI_START
	mtk_spi_write(0x00021000, 0x00000001); //DSI_START
	while ((mtk_spi_read(0x0002100c) & 0x2) != 0x2) { //wait dsi is not busy
		udelay(DELAY_US);
	}
	mtk_spi_write(0x0002100c, 0xfffd); //write 0 clear
	udelay(0);

	//lcm_dcs_write_seq_static(ctx, 0X0F, 0X01);
	mtk_spi_write(0x00021d00, 0x010f1500);
	mtk_spi_write(0x00021060, 0x00000001); //DSI_CMDQ_CON
	mtk_spi_write(0x00021000, 0x00000000); //DSI_START
	mtk_spi_write(0x00021000, 0x00000001); //DSI_START
	while ((mtk_spi_read(0x0002100c) & 0x2) != 0x2) { //wait dsi is not busy
		udelay(DELAY_US);
	}
	mtk_spi_write(0x0002100c, 0xfffd); //write 0 clear
	udelay(0);

	//lcm_dcs_write_seq_static(ctx, 0X11, 0XE0);
	mtk_spi_write(0x00021d00, 0xe0111500);
	mtk_spi_write(0x00021060, 0x00000001); //DSI_CMDQ_CON
	mtk_spi_write(0x00021000, 0x00000000); //DSI_START
	mtk_spi_write(0x00021000, 0x00000001); //DSI_START
	while ((mtk_spi_read(0x0002100c) & 0x2) != 0x2) { //wait dsi is not busy
		udelay(DELAY_US);
	}
	mtk_spi_write(0x0002100c, 0xfffd); //write 0 clear
	udelay(0);

	//lcm_dcs_write_seq_static(ctx, 0X15, 0X0E);
	mtk_spi_write(0x00021d00, 0x0e151500);
	mtk_spi_write(0x00021060, 0x00000001); //DSI_CMDQ_CON
	mtk_spi_write(0x00021000, 0x00000000); //DSI_START
	mtk_spi_write(0x00021000, 0x00000001); //DSI_START
	while ((mtk_spi_read(0x0002100c) & 0x2) != 0x2) { //wait dsi is not busy
		udelay(DELAY_US);
	}
	mtk_spi_write(0x0002100c, 0xfffd); //write 0 clear
	udelay(0);

	//lcm_dcs_write_seq_static(ctx, 0X16, 0X78);
	mtk_spi_write(0x00021d00, 0x78161500);
	mtk_spi_write(0x00021060, 0x00000001); //DSI_CMDQ_CON
	mtk_spi_write(0x00021000, 0x00000000); //DSI_START
	mtk_spi_write(0x00021000, 0x00000001); //DSI_START
	while ((mtk_spi_read(0x0002100c) & 0x2) != 0x2) { //wait dsi is not busy
		udelay(DELAY_US);
	}
	mtk_spi_write(0x0002100c, 0xfffd); //write 0 clear
	udelay(0);

	//lcm_dcs_write_seq_static(ctx, 0X19, 0X0D);
	mtk_spi_write(0x00021d00, 0x0d191500);
	mtk_spi_write(0x00021060, 0x00000001); //DSI_CMDQ_CON
	mtk_spi_write(0x00021000, 0x00000000); //DSI_START
	mtk_spi_write(0x00021000, 0x00000001); //DSI_START
	while ((mtk_spi_read(0x0002100c) & 0x2) != 0x2) { //wait dsi is not busy
		udelay(DELAY_US);
	}
	mtk_spi_write(0x0002100c, 0xfffd); //write 0 clear
	udelay(0);

	//lcm_dcs_write_seq_static(ctx, 0X1A, 0XF4);
	mtk_spi_write(0x00021d00, 0xf41a1500);
	mtk_spi_write(0x00021060, 0x00000001); //DSI_CMDQ_CON
	mtk_spi_write(0x00021000, 0x00000000); //DSI_START
	mtk_spi_write(0x00021000, 0x00000001); //DSI_START
	while ((mtk_spi_read(0x0002100c) & 0x2) != 0x2) { //wait dsi is not busy
		udelay(DELAY_US);
	}
	mtk_spi_write(0x0002100c, 0xfffd); //write 0 clear
	udelay(0);

	//lcm_dcs_write_seq_static(ctx, 0X37, 0X6E);
	mtk_spi_write(0x00021d00, 0x6e371500);
	mtk_spi_write(0x00021060, 0x00000001); //DSI_CMDQ_CON
	mtk_spi_write(0x00021000, 0x00000000); //DSI_START
	mtk_spi_write(0x00021000, 0x00000001); //DSI_START
	while ((mtk_spi_read(0x0002100c) & 0x2) != 0x2) { //wait dsi is not busy
		udelay(DELAY_US);
	}
	mtk_spi_write(0x0002100c, 0xfffd); //write 0 clear
	udelay(0);

	//lcm_dcs_write_seq_static(ctx, 0X88, 0X76);
	mtk_spi_write(0x00021d00, 0x76881500);
	mtk_spi_write(0x00021060, 0x00000001); //DSI_CMDQ_CON
	mtk_spi_write(0x00021000, 0x00000000); //DSI_START
	mtk_spi_write(0x00021000, 0x00000001); //DSI_START
	while ((mtk_spi_read(0x0002100c) & 0x2) != 0x2) { //wait dsi is not busy
		udelay(DELAY_US);
	}
	mtk_spi_write(0x0002100c, 0xfffd); //write 0 clear
	udelay(0);

	//lcm_dcs_write_seq_static(ctx, 0XFF, 0X2C);
	mtk_spi_write(0x00021d00, 0x2cff2300);
	mtk_spi_write(0x00021060, 0x00000001); //DSI_CMDQ_CON
	mtk_spi_write(0x00021000, 0x00000000); //DSI_START
	mtk_spi_write(0x00021000, 0x00000001); //DSI_START
	while ((mtk_spi_read(0x0002100c) & 0x2) != 0x2) { //wait dsi is not busy
		udelay(DELAY_US);
	}
	mtk_spi_write(0x0002100c, 0xfffd); //write 0 clear
	udelay(0);

	//lcm_dcs_write_seq_static(ctx, 0XFB, 0X01);
	mtk_spi_write(0x00021d00, 0x01fb2300);
	mtk_spi_write(0x00021060, 0x00000001); //DSI_CMDQ_CON
	mtk_spi_write(0x00021000, 0x00000000); //DSI_START
	mtk_spi_write(0x00021000, 0x00000001); //DSI_START
	while ((mtk_spi_read(0x0002100c) & 0x2) != 0x2) { //wait dsi is not busy
		udelay(DELAY_US);
	}
	mtk_spi_write(0x0002100c, 0xfffd); //write 0 clear
	udelay(0);

	//lcm_dcs_write_seq_static(ctx, 0X4D, 0X1E);
	mtk_spi_write(0x00021d00, 0x1e4d1500);
	mtk_spi_write(0x00021060, 0x00000001); //DSI_CMDQ_CON
	mtk_spi_write(0x00021000, 0x00000000); //DSI_START
	mtk_spi_write(0x00021000, 0x00000001); //DSI_START
	while ((mtk_spi_read(0x0002100c) & 0x2) != 0x2) { //wait dsi is not busy
		udelay(DELAY_US);
	}
	mtk_spi_write(0x0002100c, 0xfffd); //write 0 clear
	udelay(0);

	//lcm_dcs_write_seq_static(ctx, 0X4E, 0X04);
	mtk_spi_write(0x00021d00, 0x044e1500);
	mtk_spi_write(0x00021060, 0x00000001); //DSI_CMDQ_CON
	mtk_spi_write(0x00021000, 0x00000000); //DSI_START
	mtk_spi_write(0x00021000, 0x00000001); //DSI_START
	while ((mtk_spi_read(0x0002100c) & 0x2) != 0x2) { //wait dsi is not busy
		udelay(DELAY_US);
	}
	mtk_spi_write(0x0002100c, 0xfffd); //write 0 clear
	udelay(0);

	//lcm_dcs_write_seq_static(ctx, 0X4F, 0X00);
	mtk_spi_write(0x00021d00, 0x004f1500);
	mtk_spi_write(0x00021060, 0x00000001); //DSI_CMDQ_CON
	mtk_spi_write(0x00021000, 0x00000000); //DSI_START
	mtk_spi_write(0x00021000, 0x00000001); //DSI_START
	while ((mtk_spi_read(0x0002100c) & 0x2) != 0x2) { //wait dsi is not busy
		udelay(DELAY_US);
	}
	mtk_spi_write(0x0002100c, 0xfffd); //write 0 clear
	udelay(0);

	//lcm_dcs_write_seq_static(ctx, 0X9D, 0X1E);
	mtk_spi_write(0x00021d00, 0x1e9d1500);
	mtk_spi_write(0x00021060, 0x00000001); //DSI_CMDQ_CON
	mtk_spi_write(0x00021000, 0x00000000); //DSI_START
	mtk_spi_write(0x00021000, 0x00000001); //DSI_START
	while ((mtk_spi_read(0x0002100c) & 0x2) != 0x2) { //wait dsi is not busy
		udelay(DELAY_US);
	}
	mtk_spi_write(0x0002100c, 0xfffd); //write 0 clear
	udelay(0);

	//lcm_dcs_write_seq_static(ctx, 0X9E, 0X04);
	mtk_spi_write(0x00021d00, 0x049e1500);
	mtk_spi_write(0x00021060, 0x00000001); //DSI_CMDQ_CON
	mtk_spi_write(0x00021000, 0x00000000); //DSI_START
	mtk_spi_write(0x00021000, 0x00000001); //DSI_START
	while ((mtk_spi_read(0x0002100c) & 0x2) != 0x2) { //wait dsi is not busy
		udelay(DELAY_US);
	}
	mtk_spi_write(0x0002100c, 0xfffd); //write 0 clear
	udelay(0);

	//lcm_dcs_write_seq_static(ctx, 0X9F, 0X17);
	mtk_spi_write(0x00021d00, 0x179f1500);
	mtk_spi_write(0x00021060, 0x00000001); //DSI_CMDQ_CON
	mtk_spi_write(0x00021000, 0x00000000); //DSI_START
	mtk_spi_write(0x00021000, 0x00000001); //DSI_START
	while ((mtk_spi_read(0x0002100c) & 0x2) != 0x2) { //wait dsi is not busy
		udelay(DELAY_US);
	}
	mtk_spi_write(0x0002100c, 0xfffd); //write 0 clear
	udelay(0);

	//lcm_dcs_write_seq_static(ctx, 0XFF, 0XF0);
	mtk_spi_write(0x00021d00, 0xf0ff2300);
	mtk_spi_write(0x00021060, 0x00000001); //DSI_CMDQ_CON
	mtk_spi_write(0x00021000, 0x00000000); //DSI_START
	mtk_spi_write(0x00021000, 0x00000001); //DSI_START
	while ((mtk_spi_read(0x0002100c) & 0x2) != 0x2) { //wait dsi is not busy
		udelay(DELAY_US);
	}
	mtk_spi_write(0x0002100c, 0xfffd); //write 0 clear
	udelay(0);

	//lcm_dcs_write_seq_static(ctx, 0XFB, 0X01);
	mtk_spi_write(0x00021d00, 0x01fb2300);
	mtk_spi_write(0x00021060, 0x00000001); //DSI_CMDQ_CON
	mtk_spi_write(0x00021000, 0x00000000); //DSI_START
	mtk_spi_write(0x00021000, 0x00000001); //DSI_START
	while ((mtk_spi_read(0x0002100c) & 0x2) != 0x2) { //wait dsi is not busy
		udelay(DELAY_US);
	}
	mtk_spi_write(0x0002100c, 0xfffd); //write 0 clear
	udelay(0);

	//lcm_dcs_write_seq_static(ctx, 0X5A, 0X00);
	mtk_spi_write(0x00021d00, 0x005a1500);
	mtk_spi_write(0x00021060, 0x00000001); //DSI_CMDQ_CON
	mtk_spi_write(0x00021000, 0x00000000); //DSI_START
	mtk_spi_write(0x00021000, 0x00000001); //DSI_START
	while ((mtk_spi_read(0x0002100c) & 0x2) != 0x2) { //wait dsi is not busy
		udelay(DELAY_US);
	}
	mtk_spi_write(0x0002100c, 0xfffd); //write 0 clear
	udelay(0);

	//lcm_dcs_write_seq_static(ctx, 0XFF, 0XE0);
	mtk_spi_write(0x00021d00, 0xe0ff2300);
	mtk_spi_write(0x00021060, 0x00000001); //DSI_CMDQ_CON
	mtk_spi_write(0x00021000, 0x00000000); //DSI_START
	mtk_spi_write(0x00021000, 0x00000001); //DSI_START
	while ((mtk_spi_read(0x0002100c) & 0x2) != 0x2) { //wait dsi is not busy
		udelay(DELAY_US);
	}
	mtk_spi_write(0x0002100c, 0xfffd); //write 0 clear
	udelay(0);

	//lcm_dcs_write_seq_static(ctx, 0XFB, 0X01);
	mtk_spi_write(0x00021d00, 0x01fb2300);
	mtk_spi_write(0x00021060, 0x00000001); //DSI_CMDQ_CON
	mtk_spi_write(0x00021000, 0x00000000); //DSI_START
	mtk_spi_write(0x00021000, 0x00000001); //DSI_START
	while ((mtk_spi_read(0x0002100c) & 0x2) != 0x2) { //wait dsi is not busy
		udelay(DELAY_US);
	}
	mtk_spi_write(0x0002100c, 0xfffd); //write 0 clear
	udelay(0);

	//lcm_dcs_write_seq_static(ctx, 0X25, 0X02);
	mtk_spi_write(0x00021d00, 0x02251500);
	mtk_spi_write(0x00021060, 0x00000001); //DSI_CMDQ_CON
	mtk_spi_write(0x00021000, 0x00000000); //DSI_START
	mtk_spi_write(0x00021000, 0x00000001); //DSI_START
	while ((mtk_spi_read(0x0002100c) & 0x2) != 0x2) { //wait dsi is not busy
		udelay(DELAY_US);
	}
	mtk_spi_write(0x0002100c, 0xfffd); //write 0 clear
	udelay(0);

	//lcm_dcs_write_seq_static(ctx, 0X4E, 0X02);
	mtk_spi_write(0x00021d00, 0x024e1500);
	mtk_spi_write(0x00021060, 0x00000001); //DSI_CMDQ_CON
	mtk_spi_write(0x00021000, 0x00000000); //DSI_START
	mtk_spi_write(0x00021000, 0x00000001); //DSI_START
	while ((mtk_spi_read(0x0002100c) & 0x2) != 0x2) { //wait dsi is not busy
		udelay(DELAY_US);
	}
	mtk_spi_write(0x0002100c, 0xfffd); //write 0 clear
	udelay(0);

	//lcm_dcs_write_seq_static(ctx, 0X85, 0X02);
	mtk_spi_write(0x00021d00, 0x02851500);
	mtk_spi_write(0x00021060, 0x00000001); //DSI_CMDQ_CON
	mtk_spi_write(0x00021000, 0x00000000); //DSI_START
	mtk_spi_write(0x00021000, 0x00000001); //DSI_START
	while ((mtk_spi_read(0x0002100c) & 0x2) != 0x2) { //wait dsi is not busy
		udelay(DELAY_US);
	}
	mtk_spi_write(0x0002100c, 0xfffd); //write 0 clear
	udelay(0);

	//lcm_dcs_write_seq_static(ctx, 0XFF, 0XD0);
	mtk_spi_write(0x00021d00, 0xd0ff2300);
	mtk_spi_write(0x00021060, 0x00000001); //DSI_CMDQ_CON
	mtk_spi_write(0x00021000, 0x00000000); //DSI_START
	mtk_spi_write(0x00021000, 0x00000001); //DSI_START
	while ((mtk_spi_read(0x0002100c) & 0x2) != 0x2) { //wait dsi is not busy
		udelay(DELAY_US);
	}
	mtk_spi_write(0x0002100c, 0xfffd); //write 0 clear
	udelay(0);

	//lcm_dcs_write_seq_static(ctx, 0XFB, 0X01);
	mtk_spi_write(0x00021d00, 0x01fb2300);
	mtk_spi_write(0x00021060, 0x00000001); //DSI_CMDQ_CON
	mtk_spi_write(0x00021000, 0x00000000); //DSI_START
	mtk_spi_write(0x00021000, 0x00000001); //DSI_START
	while ((mtk_spi_read(0x0002100c) & 0x2) != 0x2) { //wait dsi is not busy
		udelay(DELAY_US);
	}
	mtk_spi_write(0x0002100c, 0xfffd); //write 0 clear
	udelay(0);

	//lcm_dcs_write_seq_static(ctx, 0X09, 0XAD);
	mtk_spi_write(0x00021d00, 0xad091500);
	mtk_spi_write(0x00021060, 0x00000001); //DSI_CMDQ_CON
	mtk_spi_write(0x00021000, 0x00000000); //DSI_START
	mtk_spi_write(0x00021000, 0x00000001); //DSI_START
	while ((mtk_spi_read(0x0002100c) & 0x2) != 0x2) { //wait dsi is not busy
		udelay(DELAY_US);
	}
	mtk_spi_write(0x0002100c, 0xfffd); //write 0 clear
	udelay(0);

	//lcm_dcs_write_seq_static(ctx, 0XFF, 0X20);
	mtk_spi_write(0x00021d00, 0x20ff2300);
	mtk_spi_write(0x00021060, 0x00000001); //DSI_CMDQ_CON
	mtk_spi_write(0x00021000, 0x00000000); //DSI_START
	mtk_spi_write(0x00021000, 0x00000001); //DSI_START
	while ((mtk_spi_read(0x0002100c) & 0x2) != 0x2) { //wait dsi is not busy
		udelay(DELAY_US);
	}
	mtk_spi_write(0x0002100c, 0xfffd); //write 0 clear
	udelay(0);

	//lcm_dcs_write_seq_static(ctx, 0XFB, 0X01);
	mtk_spi_write(0x00021d00, 0x01fb2300);
	mtk_spi_write(0x00021060, 0x00000001); //DSI_CMDQ_CON
	mtk_spi_write(0x00021000, 0x00000000); //DSI_START
	mtk_spi_write(0x00021000, 0x00000001); //DSI_START
	while ((mtk_spi_read(0x0002100c) & 0x2) != 0x2) { //wait dsi is not busy
		udelay(DELAY_US);
	}
	mtk_spi_write(0x0002100c, 0xfffd); //write 0 clear
	udelay(0);

	//lcm_dcs_write_seq_static(ctx, 0XF8, 0X64);
	mtk_spi_write(0x00021d00, 0x64f82300);
	mtk_spi_write(0x00021060, 0x00000001); //DSI_CMDQ_CON
	mtk_spi_write(0x00021000, 0x00000000); //DSI_START
	mtk_spi_write(0x00021000, 0x00000001); //DSI_START
	while ((mtk_spi_read(0x0002100c) & 0x2) != 0x2) { //wait dsi is not busy
		udelay(DELAY_US);
	}
	mtk_spi_write(0x0002100c, 0xfffd); //write 0 clear
	udelay(0);

	//lcm_dcs_write_seq_static(ctx, 0XFF, 0X2A);
	mtk_spi_write(0x00021d00, 0x2aff2300);
	mtk_spi_write(0x00021060, 0x00000001); //DSI_CMDQ_CON
	mtk_spi_write(0x00021000, 0x00000000); //DSI_START
	mtk_spi_write(0x00021000, 0x00000001); //DSI_START
	while ((mtk_spi_read(0x0002100c) & 0x2) != 0x2) { //wait dsi is not busy
		udelay(DELAY_US);
	}
	mtk_spi_write(0x0002100c, 0xfffd); //write 0 clear
	udelay(0);

	//lcm_dcs_write_seq_static(ctx, 0XFB, 0X01);
	mtk_spi_write(0x00021d00, 0x01fb2300);
	mtk_spi_write(0x00021060, 0x00000001); //DSI_CMDQ_CON
	mtk_spi_write(0x00021000, 0x00000000); //DSI_START
	mtk_spi_write(0x00021000, 0x00000001); //DSI_START
	while ((mtk_spi_read(0x0002100c) & 0x2) != 0x2) { //wait dsi is not busy
		udelay(DELAY_US);
	}
	mtk_spi_write(0x0002100c, 0xfffd); //write 0 clear
	udelay(0);

	//lcm_dcs_write_seq_static(ctx, 0X1A, 0XF0);
	mtk_spi_write(0x00021d00, 0xf01a1500);
	mtk_spi_write(0x00021060, 0x00000001); //DSI_CMDQ_CON
	mtk_spi_write(0x00021000, 0x00000000); //DSI_START
	mtk_spi_write(0x00021000, 0x00000001); //DSI_START
	while ((mtk_spi_read(0x0002100c) & 0x2) != 0x2) { //wait dsi is not busy
		udelay(DELAY_US);
	}
	mtk_spi_write(0x0002100c, 0xfffd); //write 0 clear
	udelay(0);
	//lcm_dcs_write_seq_static(ctx, 0X30, 0X5E);
	mtk_spi_write(0x00021d00, 0x5e301500);
	mtk_spi_write(0x00021060, 0x00000001); //DSI_CMDQ_CON
	mtk_spi_write(0x00021000, 0x00000000); //DSI_START
	mtk_spi_write(0x00021000, 0x00000001); //DSI_START
	while ((mtk_spi_read(0x0002100c) & 0x2) != 0x2) { //wait dsi is not busy
		udelay(DELAY_US);
	}
	mtk_spi_write(0x0002100c, 0xfffd); //write 0 clear
	udelay(0);

	//lcm_dcs_write_seq_static(ctx, 0X31, 0XCA);
	mtk_spi_write(0x00021d00, 0xca311500);
	mtk_spi_write(0x00021060, 0x00000001); //DSI_CMDQ_CON
	mtk_spi_write(0x00021000, 0x00000000); //DSI_START
	mtk_spi_write(0x00021000, 0x00000001); //DSI_START
	while ((mtk_spi_read(0x0002100c) & 0x2) != 0x2) { //wait dsi is not busy
		udelay(DELAY_US);
	}
	mtk_spi_write(0x0002100c, 0xfffd); //write 0 clear
	udelay(0);

	//lcm_dcs_write_seq_static(ctx, 0X34, 0XFE);
	mtk_spi_write(0x00021d00, 0xfe341500);
	mtk_spi_write(0x00021060, 0x00000001); //DSI_CMDQ_CON
	mtk_spi_write(0x00021000, 0x00000000); //DSI_START
	mtk_spi_write(0x00021000, 0x00000001); //DSI_START
	while ((mtk_spi_read(0x0002100c) & 0x2) != 0x2) { //wait dsi is not busy
		udelay(DELAY_US);
	}
	mtk_spi_write(0x0002100c, 0xfffd); //write 0 clear
	udelay(0);

	//lcm_dcs_write_seq_static(ctx, 0X35, 0X35);
	mtk_spi_write(0x00021d00, 0x35351500);
	mtk_spi_write(0x00021060, 0x00000001); //DSI_CMDQ_CON
	mtk_spi_write(0x00021000, 0x00000000); //DSI_START
	mtk_spi_write(0x00021000, 0x00000001); //DSI_START
	while ((mtk_spi_read(0x0002100c) & 0x2) != 0x2) { //wait dsi is not busy
		udelay(DELAY_US);
	}
	mtk_spi_write(0x0002100c, 0xfffd); //write 0 clear
	udelay(0);

	//lcm_dcs_write_seq_static(ctx, 0X36, 0XA2);
	mtk_spi_write(0x00021d00, 0xa2361500);
	mtk_spi_write(0x00021060, 0x00000001); //DSI_CMDQ_CON
	mtk_spi_write(0x00021000, 0x00000000); //DSI_START
	mtk_spi_write(0x00021000, 0x00000001); //DSI_START
	while ((mtk_spi_read(0x0002100c) & 0x2) != 0x2) { //wait dsi is not busy
		udelay(DELAY_US);
	}
	mtk_spi_write(0x0002100c, 0xfffd); //write 0 clear
	udelay(0);

	//lcm_dcs_write_seq_static(ctx, 0X37, 0XF8);
	mtk_spi_write(0x00021d00, 0xf8371500);
	mtk_spi_write(0x00021060, 0x00000001); //DSI_CMDQ_CON
	mtk_spi_write(0x00021000, 0x00000000); //DSI_START
	mtk_spi_write(0x00021000, 0x00000001); //DSI_START
	while ((mtk_spi_read(0x0002100c) & 0x2) != 0x2) { //wait dsi is not busy
		udelay(DELAY_US);
	}
	mtk_spi_write(0x0002100c, 0xfffd); //write 0 clear
	udelay(0);

	//lcm_dcs_write_seq_static(ctx, 0X38, 0X37);
	mtk_spi_write(0x00021d00, 0x37381500);
	mtk_spi_write(0x00021060, 0x00000001); //DSI_CMDQ_CON
	mtk_spi_write(0x00021000, 0x00000000); //DSI_START
	mtk_spi_write(0x00021000, 0x00000001); //DSI_START
	while ((mtk_spi_read(0x0002100c) & 0x2) != 0x2) { //wait dsi is not busy
		udelay(DELAY_US);
	}
	mtk_spi_write(0x0002100c, 0xfffd); //write 0 clear
	udelay(0);

	//lcm_dcs_write_seq_static(ctx, 0X39, 0XA0);
	mtk_spi_write(0x00021d00, 0xa0391500);
	mtk_spi_write(0x00021060, 0x00000001); //DSI_CMDQ_CON
	mtk_spi_write(0x00021000, 0x00000000); //DSI_START
	mtk_spi_write(0x00021000, 0x00000001); //DSI_START
	while ((mtk_spi_read(0x0002100c) & 0x2) != 0x2) { //wait dsi is not busy
		udelay(DELAY_US);
	}
	mtk_spi_write(0x0002100c, 0xfffd); //write 0 clear
	udelay(0);

	//lcm_dcs_write_seq_static(ctx, 0X3A, 0X5E);
	mtk_spi_write(0x00021d00, 0x5e3a1500);
	mtk_spi_write(0x00021060, 0x00000001); //DSI_CMDQ_CON
	mtk_spi_write(0x00021000, 0x00000000); //DSI_START
	mtk_spi_write(0x00021000, 0x00000001); //DSI_START
	while ((mtk_spi_read(0x0002100c) & 0x2) != 0x2) { //wait dsi is not busy
		udelay(DELAY_US);
	}
	mtk_spi_write(0x0002100c, 0xfffd); //write 0 clear
	udelay(0);

	//lcm_dcs_write_seq_static(ctx, 0X53, 0XD7);
	mtk_spi_write(0x00021d00, 0xd7531500);
	mtk_spi_write(0x00021060, 0x00000001); //DSI_CMDQ_CON
	mtk_spi_write(0x00021000, 0x00000000); //DSI_START
	mtk_spi_write(0x00021000, 0x00000001); //DSI_START
	while ((mtk_spi_read(0x0002100c) & 0x2) != 0x2) { //wait dsi is not busy
		udelay(DELAY_US);
	}
	mtk_spi_write(0x0002100c, 0xfffd); //write 0 clear
	udelay(0);

	//lcm_dcs_write_seq_static(ctx, 0X88, 0X72);
	mtk_spi_write(0x00021d00, 0x72881500);
	mtk_spi_write(0x00021060, 0x00000001); //DSI_CMDQ_CON
	mtk_spi_write(0x00021000, 0x00000000); //DSI_START
	mtk_spi_write(0x00021000, 0x00000001); //DSI_START
	while ((mtk_spi_read(0x0002100c) & 0x2) != 0x2) { //wait dsi is not busy
		udelay(DELAY_US);
	}
	mtk_spi_write(0x0002100c, 0xfffd); //write 0 clear
	udelay(0);

	//lcm_dcs_write_seq_static(ctx, 0X88, 0X72);
	mtk_spi_write(0x00021d00, 0x72881500);
	mtk_spi_write(0x00021060, 0x00000001); //DSI_CMDQ_CON
	mtk_spi_write(0x00021000, 0x00000000); //DSI_START
	mtk_spi_write(0x00021000, 0x00000001); //DSI_START
	while ((mtk_spi_read(0x0002100c) & 0x2) != 0x2) { //wait dsi is not busy
		udelay(DELAY_US);
	}
	mtk_spi_write(0x0002100c, 0xfffd); //write 0 clear
	udelay(0);

	//lcm_dcs_write_seq_static(ctx, 0XFF, 0X24);
	mtk_spi_write(0x00021d00, 0x24ff2300);
	mtk_spi_write(0x00021060, 0x00000001); //DSI_CMDQ_CON
	mtk_spi_write(0x00021000, 0x00000000); //DSI_START
	mtk_spi_write(0x00021000, 0x00000001); //DSI_START
	while ((mtk_spi_read(0x0002100c) & 0x2) != 0x2) { //wait dsi is not busy
		udelay(DELAY_US);
	}
	mtk_spi_write(0x0002100c, 0xfffd); //write 0 clear
	udelay(0);

	//lcm_dcs_write_seq_static(ctx, 0XFB, 0X01);
	mtk_spi_write(0x00021d00, 0x01fb2300);
	mtk_spi_write(0x00021060, 0x00000001); //DSI_CMDQ_CON
	mtk_spi_write(0x00021000, 0x00000000); //DSI_START
	mtk_spi_write(0x00021000, 0x00000001); //DSI_START
	while ((mtk_spi_read(0x0002100c) & 0x2) != 0x2) { //wait dsi is not busy
		udelay(DELAY_US);
	}
	mtk_spi_write(0x0002100c, 0xfffd); //write 0 clear
	udelay(0);

	//lcm_dcs_write_seq_static(ctx, 0XC6, 0XC0);
	mtk_spi_write(0x00021d00, 0xc0c62300);
	mtk_spi_write(0x00021060, 0x00000001); //DSI_CMDQ_CON
	mtk_spi_write(0x00021000, 0x00000000); //DSI_START
	mtk_spi_write(0x00021000, 0x00000001); //DSI_START
	while ((mtk_spi_read(0x0002100c) & 0x2) != 0x2) { //wait dsi is not busy
		udelay(DELAY_US);
	}
	mtk_spi_write(0x0002100c, 0xfffd); //write 0 clear
	udelay(0);

	//lcm_dcs_write_seq_static(ctx, 0XFF, 0XE0);
	mtk_spi_write(0x00021d00, 0xe0ff2300);
	mtk_spi_write(0x00021060, 0x00000001); //DSI_CMDQ_CON
	mtk_spi_write(0x00021000, 0x00000000); //DSI_START
	mtk_spi_write(0x00021000, 0x00000001); //DSI_START
	while ((mtk_spi_read(0x0002100c) & 0x2) != 0x2) { //wait dsi is not busy
		udelay(DELAY_US);
	}
	mtk_spi_write(0x0002100c, 0xfffd); //write 0 clear
	udelay(0);

	//lcm_dcs_write_seq_static(ctx, 0XFB, 0X01);
	mtk_spi_write(0x00021d00, 0x01fb2300);
	mtk_spi_write(0x00021060, 0x00000001); //DSI_CMDQ_CON
	mtk_spi_write(0x00021000, 0x00000000); //DSI_START
	mtk_spi_write(0x00021000, 0x00000001); //DSI_START
	while ((mtk_spi_read(0x0002100c) & 0x2) != 0x2) { //wait dsi is not busy
		udelay(DELAY_US);
	}
	mtk_spi_write(0x0002100c, 0xfffd); //write 0 clear
	udelay(0);

	//lcm_dcs_write_seq_static(ctx, 0X25, 0X00);
	mtk_spi_write(0x00021d00, 0x00251500);
	mtk_spi_write(0x00021060, 0x00000001); //DSI_CMDQ_CON
	mtk_spi_write(0x00021000, 0x00000000); //DSI_START
	mtk_spi_write(0x00021000, 0x00000001); //DSI_START
	while ((mtk_spi_read(0x0002100c) & 0x2) != 0x2) { //wait dsi is not busy
		udelay(DELAY_US);
	}
	mtk_spi_write(0x0002100c, 0xfffd); //write 0 clear
	udelay(0);

	//lcm_dcs_write_seq_static(ctx, 0X4E, 0X02);
	mtk_spi_write(0x00021d00, 0x024e1500);
	mtk_spi_write(0x00021060, 0x00000001); //DSI_CMDQ_CON
	mtk_spi_write(0x00021000, 0x00000000); //DSI_START
	mtk_spi_write(0x00021000, 0x00000001); //DSI_START
	while ((mtk_spi_read(0x0002100c) & 0x2) != 0x2) { //wait dsi is not busy
		udelay(DELAY_US);
	}
	mtk_spi_write(0x0002100c, 0xfffd); //write 0 clear
	udelay(0);

	//lcm_dcs_write_seq_static(ctx, 0X35, 0X82);
	mtk_spi_write(0x00021d00, 0x82351500);
	mtk_spi_write(0x00021060, 0x00000001); //DSI_CMDQ_CON
	mtk_spi_write(0x00021000, 0x00000000); //DSI_START
	mtk_spi_write(0x00021000, 0x00000001); //DSI_START
	while ((mtk_spi_read(0x0002100c) & 0x2) != 0x2) { //wait dsi is not busy
		udelay(DELAY_US);
	}
	mtk_spi_write(0x0002100c, 0xfffd); //write 0 clear
	udelay(0);

	//lcm_dcs_write_seq_static(ctx, 0XFF, 0XC0);
	mtk_spi_write(0x00021d00, 0xc0ff2300);
	mtk_spi_write(0x00021060, 0x00000001); //DSI_CMDQ_CON
	mtk_spi_write(0x00021000, 0x00000000); //DSI_START
	mtk_spi_write(0x00021000, 0x00000001); //DSI_START
	while ((mtk_spi_read(0x0002100c) & 0x2) != 0x2) { //wait dsi is not busy
		udelay(DELAY_US);
	}
	mtk_spi_write(0x0002100c, 0xfffd); //write 0 clear
	udelay(0);

	//lcm_dcs_write_seq_static(ctx, 0XFB, 0X01);
	mtk_spi_write(0x00021d00, 0x01fb2300);
	mtk_spi_write(0x00021060, 0x00000001); //DSI_CMDQ_CON
	mtk_spi_write(0x00021000, 0x00000000); //DSI_START
	mtk_spi_write(0x00021000, 0x00000001); //DSI_START
	while ((mtk_spi_read(0x0002100c) & 0x2) != 0x2) { //wait dsi is not busy
		udelay(DELAY_US);
	}
	mtk_spi_write(0x0002100c, 0xfffd); //write 0 clear
	udelay(0);

	//lcm_dcs_write_seq_static(ctx, 0X9C, 0X11);
	mtk_spi_write(0x00021d00, 0x119c1500);
	mtk_spi_write(0x00021060, 0x00000001); //DSI_CMDQ_CON
	mtk_spi_write(0x00021000, 0x00000000); //DSI_START
	mtk_spi_write(0x00021000, 0x00000001); //DSI_START
	while ((mtk_spi_read(0x0002100c) & 0x2) != 0x2) { //wait dsi is not busy
		udelay(DELAY_US);
	}
	mtk_spi_write(0x0002100c, 0xfffd); //write 0 clear
	udelay(0);

	//lcm_dcs_write_seq_static(ctx, 0X9D, 0X11);
	mtk_spi_write(0x00021d00, 0x119d1500);
	mtk_spi_write(0x00021060, 0x00000001); //DSI_CMDQ_CON
	mtk_spi_write(0x00021000, 0x00000000); //DSI_START
	mtk_spi_write(0x00021000, 0x00000001); //DSI_START
	while ((mtk_spi_read(0x0002100c) & 0x2) != 0x2) { //wait dsi is not busy
		udelay(DELAY_US);
	}
	mtk_spi_write(0x0002100c, 0xfffd); //write 0 clear
	udelay(0);

	if (dsc_on) {
		if (tx_data_rate < 600) {
		//60HZ
			//lcm_dcs_write_seq_static(ctx, 0XFF, 0X25);
			mtk_spi_write(0x00021d00, 0x25ff2300);
			mtk_spi_write(0x00021060, 0x00000001); //DSI_CMDQ_CON
			mtk_spi_write(0x00021000, 0x00000000); //DSI_START
			mtk_spi_write(0x00021000, 0x00000001); //DSI_START
			while ((mtk_spi_read(0x0002100c) & 0x2) != 0x2) { //wait dsi is not busy
				udelay(DELAY_US);
			}
			mtk_spi_write(0x0002100c, 0xfffd); //write 0 clear
			udelay(0);

			//lcm_dcs_write_seq_static(ctx, 0XFB, 0X01);
			mtk_spi_write(0x00021d00, 0x01fb2300);
			mtk_spi_write(0x00021060, 0x00000001); //DSI_CMDQ_CON
			mtk_spi_write(0x00021000, 0x00000000); //DSI_START
			mtk_spi_write(0x00021000, 0x00000001); //DSI_START
			while ((mtk_spi_read(0x0002100c) & 0x2) != 0x2) { //wait dsi is not busy
				udelay(DELAY_US);
			}
			mtk_spi_write(0x0002100c, 0xfffd); //write 0 clear
			udelay(0);

			//lcm_dcs_write_seq_static(ctx, 0X18, 0X22);
			mtk_spi_write(0x00021d00, 0x22181500);
			mtk_spi_write(0x00021060, 0x00000001); //DSI_CMDQ_CON
			mtk_spi_write(0x00021000, 0x00000000); //DSI_START
			mtk_spi_write(0x00021000, 0x00000001); //DSI_START
			while ((mtk_spi_read(0x0002100c) & 0x2) != 0x2) { //wait dsi is not busy
				udelay(DELAY_US);
			}
			mtk_spi_write(0x0002100c, 0xfffd); //write 0 clear
			udelay(0);
		} else if (tx_data_rate < 900) {
		//90HZ
			//lcm_dcs_write_seq_static(ctx, 0XFF, 0X25);
			mtk_spi_write(0x00021d00, 0x25ff2300);
			mtk_spi_write(0x00021060, 0x00000001); //DSI_CMDQ_CON
			mtk_spi_write(0x00021000, 0x00000000); //DSI_START
			mtk_spi_write(0x00021000, 0x00000001); //DSI_START
			while ((mtk_spi_read(0x0002100c) & 0x2) != 0x2) { //wait dsi is not busy
				udelay(DELAY_US);
			}
			mtk_spi_write(0x0002100c, 0xfffd); //write 0 clear
			udelay(0);

			//lcm_dcs_write_seq_static(ctx, 0XFB, 0X01);
			mtk_spi_write(0x00021d00, 0x01fb2300);
			mtk_spi_write(0x00021060, 0x00000001); //DSI_CMDQ_CON
			mtk_spi_write(0x00021000, 0x00000000); //DSI_START
			mtk_spi_write(0x00021000, 0x00000001); //DSI_START
			while ((mtk_spi_read(0x0002100c) & 0x2) != 0x2) { //wait dsi is not busy
				udelay(DELAY_US);
			}
			mtk_spi_write(0x0002100c, 0xfffd); //write 0 clear
			udelay(0);

			//lcm_dcs_write_seq_static(ctx, 0X18, 0X21);
			mtk_spi_write(0x00021d00, 0x21181500);
			mtk_spi_write(0x00021060, 0x00000001); //DSI_CMDQ_CON
			mtk_spi_write(0x00021000, 0x00000000); //DSI_START
			mtk_spi_write(0x00021000, 0x00000001); //DSI_START
			while ((mtk_spi_read(0x0002100c) & 0x2) != 0x2) { //wait dsi is not busy
				udelay(DELAY_US);
			}
			mtk_spi_write(0x0002100c, 0xfffd); //write 0 clear
			udelay(0);
		}
	} else {
		//60HZ
		//lcm_dcs_write_seq_static(ctx, 0XFF, 0X25);
		mtk_spi_write(0x00021d00, 0x25ff2300);
		mtk_spi_write(0x00021060, 0x00000001); //DSI_CMDQ_CON
		mtk_spi_write(0x00021000, 0x00000000); //DSI_START
		mtk_spi_write(0x00021000, 0x00000001); //DSI_START
		while ((mtk_spi_read(0x0002100c) & 0x2) != 0x2) { //wait dsi is not busy
			udelay(DELAY_US);
		}
		mtk_spi_write(0x0002100c, 0xfffd); //write 0 clear
		udelay(0);

		//lcm_dcs_write_seq_static(ctx, 0XFB, 0X01);
		mtk_spi_write(0x00021d00, 0x01fb2300);
		mtk_spi_write(0x00021060, 0x00000001); //DSI_CMDQ_CON
		mtk_spi_write(0x00021000, 0x00000000); //DSI_START
		mtk_spi_write(0x00021000, 0x00000001); //DSI_START
		while ((mtk_spi_read(0x0002100c) & 0x2) != 0x2) { //wait dsi is not busy
			udelay(DELAY_US);
		}
		mtk_spi_write(0x0002100c, 0xfffd); //write 0 clear
		udelay(0);

		//lcm_dcs_write_seq_static(ctx, 0X18, 0X22);
		mtk_spi_write(0x00021d00, 0x22181500);
		mtk_spi_write(0x00021060, 0x00000001); //DSI_CMDQ_CON
		mtk_spi_write(0x00021000, 0x00000000); //DSI_START
		mtk_spi_write(0x00021000, 0x00000001); //DSI_START
		while ((mtk_spi_read(0x0002100c) & 0x2) != 0x2) { //wait dsi is not busy
			udelay(DELAY_US);
		}
		mtk_spi_write(0x0002100c, 0xfffd); //write 0 clear
		udelay(0);
	}

	//lcm_dcs_write_seq_static(ctx, 0XFF, 0X10);
	mtk_spi_write(0x00021d00, 0x10ff2300);
	mtk_spi_write(0x00021060, 0x00000001); //DSI_CMDQ_CON
	mtk_spi_write(0x00021000, 0x00000000); //DSI_START
	mtk_spi_write(0x00021000, 0x00000001); //DSI_START
	while ((mtk_spi_read(0x0002100c) & 0x2) != 0x2) { //wait dsi is not busy
		udelay(DELAY_US);
	}
	mtk_spi_write(0x0002100c, 0xfffd); //write 0 clear
	udelay(0);

	//lcm_dcs_write_seq_static(ctx, 0XFB, 0X01);
	mtk_spi_write(0x00021d00, 0x01fb2300);
	mtk_spi_write(0x00021060, 0x00000001); //DSI_CMDQ_CON
	mtk_spi_write(0x00021000, 0x00000000); //DSI_START
	mtk_spi_write(0x00021000, 0x00000001); //DSI_START
	while ((mtk_spi_read(0x0002100c) & 0x2) != 0x2) { //wait dsi is not busy
		udelay(DELAY_US);
	}
	mtk_spi_write(0x0002100c, 0xfffd); //write 0 clear
	udelay(0);

	//Set DSC ON/OFF
	if (dsc_on) {
		//lcm_dcs_write_seq_static(ctx, 0xC0, 0x03);
		mtk_spi_write(0x00021d00, 0x03c02300);
		mtk_spi_write(0x00021060, 0x00000001); //DSI_CMDQ_CON
		mtk_spi_write(0x00021000, 0x00000000); //DSI_START
		mtk_spi_write(0x00021000, 0x00000001); //DSI_START
		while ((mtk_spi_read(0x0002100c) & 0x2) != 0x2) { //wait dsi is not busy
			DDPMSG("%s, status=0x%x\n",
				__func__, mtk_spi_read((unsigned long)(&TX_REG[0]->DSI_TX_INTSTA)));
			udelay(DELAY_US);
		}
		mtk_spi_write(0x0002100c, 0xfffd); //write 0 clear
		udelay(0);
	} else {
		//lcm_dcs_write_seq_static(ctx, 0xC0, 0x00);
		mtk_spi_write(0x00021d00, 0x00c02300);
		mtk_spi_write(0x00021060, 0x00000001); //DSI_CMDQ_CON
		mtk_spi_write(0x00021000, 0x00000000); //DSI_START
		mtk_spi_write(0x00021000, 0x00000001); //DSI_START
		while ((mtk_spi_read(0x0002100c) & 0x2) != 0x2) { //wait dsi is not busy
			udelay(DELAY_US);
		}
		mtk_spi_write(0x0002100c, 0xfffd); //write 0 clear
		udelay(0);
	}

	mtk_spi_write(0x00021060, 0x00000001); //DSI_CMDQ_CON
	mtk_spi_write(0x00021000, 0x00000000); //DSI_START
	mtk_spi_write(0x00021000, 0x00000001); //DSI_START
	while ((mtk_spi_read(0x0002100c) & 0x2) != 0x2) { //wait dsi is not busy
		udelay(DELAY_US);
	}
	mtk_spi_write(0x0002100c, 0xfffd); //write 0 clear
	udelay(0);

	//lcm_dcs_write_seq_static(ctx, 0x51, 0x00);
	mtk_spi_write(0x00021d00, 0x00511500);
	mtk_spi_write(0x00021060, 0x00000001); //DSI_CMDQ_CON
	mtk_spi_write(0x00021000, 0x00000000); //DSI_START
	mtk_spi_write(0x00021000, 0x00000001); //DSI_START
	while ((mtk_spi_read(0x0002100c) & 0x2) != 0x2) { //wait dsi is not busy
		udelay(DELAY_US);
	}
	mtk_spi_write(0x0002100c, 0xfffd); //write 0 clear
	udelay(0);

	//lcm_dcs_write_seq_static(ctx, 0x35, 0x00);
	mtk_spi_write(0x00021d00, 0x00351500);
	mtk_spi_write(0x00021060, 0x00000001); //DSI_CMDQ_CON
	mtk_spi_write(0x00021000, 0x00000000); //DSI_START
	mtk_spi_write(0x00021000, 0x00000001); //DSI_START
	while ((mtk_spi_read(0x0002100c) & 0x2) != 0x2) { //wait dsi is not busy
		udelay(DELAY_US);
	}
	mtk_spi_write(0x0002100c, 0xfffd); //write 0 clear
	udelay(0);

	//lcm_dcs_write_seq_static(ctx, 0x53, 0x24);
	mtk_spi_write(0x00021d00, 0x24531500);
	mtk_spi_write(0x00021060, 0x00000001); //DSI_CMDQ_CON
	mtk_spi_write(0x00021000, 0x00000000); //DSI_START
	mtk_spi_write(0x00021000, 0x00000001); //DSI_START
	while ((mtk_spi_read(0x0002100c) & 0x2) != 0x2) { //wait dsi is not busy
		udelay(DELAY_US);
	}
	mtk_spi_write(0x0002100c, 0xfffd); //write 0 clear
	udelay(0);

	//lcm_dcs_write_seq_static(ctx, 0x55, 0x00);
	mtk_spi_write(0x00021d00, 0x00551500);
	mtk_spi_write(0x00021060, 0x00000001); //DSI_CMDQ_CON
	mtk_spi_write(0x00021000, 0x00000000); //DSI_START
	mtk_spi_write(0x00021000, 0x00000001); //DSI_START
	while ((mtk_spi_read(0x0002100c) & 0x2) != 0x2) { //wait dsi is not busy
		udelay(DELAY_US);
	}
	mtk_spi_write(0x0002100c, 0xfffd); //write 0 clear
	udelay(0);

	//lcm_dcs_write_seq_static(ctx, 0xFF, 0x10);
	mtk_spi_write(0x00021d00, 0x10ff2300);
	mtk_spi_write(0x00021060, 0x00000001); //DSI_CMDQ_CON
	mtk_spi_write(0x00021000, 0x00000000); //DSI_START
	mtk_spi_write(0x00021000, 0x00000001); //DSI_START
	while ((mtk_spi_read(0x0002100c) & 0x2) != 0x2) { //wait dsi is not busy
		udelay(DELAY_US);
	}
	mtk_spi_write(0x0002100c, 0xfffd); //write 0 clear
	udelay(0);

	//lcm_dcs_write_seq_static(ctx, 0x11);
	mtk_spi_write(0x00021d00, 0x00110500);
	mtk_spi_write(0x00021060, 0x00000001); //DSI_CMDQ_CON
	mtk_spi_write(0x00021000, 0x00000000); //DSI_START
	mtk_spi_write(0x00021000, 0x00000001); //DSI_START
	while ((mtk_spi_read(0x0002100c) & 0x2) != 0x2) { //wait dsi is not busy
		udelay(DELAY_US);
	}
	mtk_spi_write(0x0002100c, 0xfffd); //write 0 clear
	udelay(0);

	msleep(120);
	//lcm_dcs_write_seq_static(ctx, 0x29);
	mtk_spi_write(0x00021d00, 0x00290500);
	mtk_spi_write(0x00021060, 0x00000001); //DSI_CMDQ_CON
	mtk_spi_write(0x00021000, 0x00000000); //DSI_START
	mtk_spi_write(0x00021000, 0x00000001); //DSI_START
	while ((mtk_spi_read(0x0002100c) & 0x2) != 0x2) { //wait dsi is not busy
		udelay(DELAY_US);
	}
	mtk_spi_write(0x0002100c, 0xfffd); //write 0 clear
	udelay(0);

}

void dbg_set_cmdq_V2(enum DISP_BDG_ENUM module, void *cmdq,
			unsigned int cmd, unsigned char count,
		     unsigned char *para_list, unsigned char force_update)
{
	u32 i = 0;
	unsigned long goto_addr, mask_para, set_para;
	struct DSI_TX_T0_INS t0;
	struct DSI_TX_T2_INS t2;
	struct DSI_TX_CMDQ *tx_cmdq;

	memset(&t0, 0, sizeof(struct DSI_TX_T0_INS));
	memset(&t2, 0, sizeof(struct DSI_TX_T2_INS));

	tx_cmdq = TX_CMDQ_REG[0]->data;

	bdg_tx_wait_for_idle(module);

	if (count > 1) {
		t2.CONFG = 2;
		if (cmd < 0xB0)
			t2.Data_ID = TX_DCS_LONG_PACKET_ID;
		else
			t2.Data_ID = TX_GERNERIC_LONG_PACKET_ID;
		t2.WC16 = count + 1;

		BDG_OUTREG32(cmdq, tx_cmdq[0], AS_UINT32(&t2));

		goto_addr = (unsigned long)(&tx_cmdq[1].byte0);
		mask_para = (0xFFu << ((goto_addr & 0x3u) * 8));
		set_para = cmd;
		goto_addr = (goto_addr & (~0x3UL));

//		DDPMSG("[%s][%d]goto_addr=0x%x, mask_para=0x%x, set_para=0x%x\n",
//			__func__, __LINE__, goto_addr, mask_para, set_para);

		BDG_MASKREG32(cmdq, goto_addr, mask_para, set_para);

		for (i = 0; i < count; i++) {
			goto_addr = (unsigned long)(&tx_cmdq[1].byte1) + i;
			mask_para = (0xFFu << ((goto_addr & 0x3u) * 8));
			set_para = (unsigned long)para_list[i];
			goto_addr = (goto_addr & (~0x3UL));

//			DDPMSG("[%s][%d]i=%d, goto_addr=0x%x, mask_para=0x%x, set_para=0x%x\n",
//				__func__, __LINE__, i, goto_addr, mask_para, set_para);

			BDG_MASKREG32(cmdq, goto_addr, mask_para, set_para);
		}

		BDG_OUTREG32(cmdq, TX_REG[0]->DSI_TX_CMDQ_CON,
				(1 << 15) | (2 + (count) / 4));
	} else {
		t0.CONFG = 0;
		t0.Data0 = cmd;
		if (count) {
			if (cmd < 0xB0)
				t0.Data_ID = TX_DCS_SHORT_PACKET_ID_1;
			else
				t0.Data_ID = TX_GERNERIC_SHORT_PACKET_ID_2;
			t0.Data1 = para_list[0];
		} else {
			if (cmd < 0xB0)
				t0.Data_ID = TX_DCS_SHORT_PACKET_ID_0;
			else
				t0.Data_ID = TX_GERNERIC_SHORT_PACKET_ID_1;
			t0.Data1 = 0;
		}

		BDG_OUTREG32(cmdq, tx_cmdq[0], AS_UINT32(&t0));
		BDG_OUTREG32(cmdq, TX_REG[0]->DSI_TX_CMDQ_CON, 1);
	}

	if (force_update) {
		bdg_tx_start(module, cmdq);
		bdg_tx_wait_for_idle(module);
	}
}

void BDG_set_cmdq_V2_DSI0(void *cmdq, unsigned int cmd, unsigned char count,
	unsigned char *para_list, unsigned char force_update)
{
	dbg_set_cmdq_V2(DISP_BDG_DSI0, cmdq, cmd, count, para_list,
		force_update);
}

int bdg_tx_init(enum DISP_BDG_ENUM module,
		   struct mtk_dsi *dsi, void *cmdq)
{
	int ret = 0;

	dsi->bdg_data_rate = mtk_dsi_default_rate(dsi);
	tx_data_rate = dsi->bdg_data_rate;
	bdg_tx_mode = dsi->mode_flags & MIPI_DSI_MODE_VIDEO;

	DDPMSG("%s, data_rate=%d,  dsc_enable=%d, mode=%d\n",
		__func__, tx_data_rate, dsc_en, bdg_tx_mode);
	ret |= bdg_mipi_tx_dphy_clk_setting(module, cmdq, dsi);
	udelay(20);

	ret |= bdg_tx_phy_config(module, cmdq, dsi);
	ret |= bdg_tx_txrx_ctrl(module, cmdq, dsi);
	ret |= bdg_tx_ps_ctrl(module, cmdq, dsi);
	ret |= bdg_tx_vdo_timing_set(module, cmdq, dsi);
	ret |= bdg_tx_buf_rw_set(module, cmdq, dsi);
	ret |= bdg_tx_enable_hs_clk(module, cmdq, true);
	ret |= bdg_tx_set_mode(module, cmdq, NULL);
	ret |= bdg_dsi_line_timing_dphy_setting(module, cmdq, dsi);

	return ret;
}

int bdg_tx_deinit(enum DISP_BDG_ENUM module, void *cmdq)
{
	int i;

	bdg_tx_enable_hs_clk(module, cmdq, false);
	bdg_tx_set_mode(module, cmdq, NULL);
	bdg_tx_reset(module, cmdq);

	for (i = DSI_MODULE_BEGIN(module); i <= DSI_MODULE_END(module); i++) {
		BDG_OUTREGBIT(cmdq, struct DSI_TX_TXRX_CON_REG,
			TX_REG[i]->DSI_TX_TXRX_CON, LANE_NUM, 0);
	}

	return 0;
}

void calculate_datarate_cfgs_rx(unsigned int data_rate)
{
	unsigned int timebase = 1000, itminrx = 4; // 1us
	unsigned int des_div_en_dly_deass_th = 1;
	unsigned int hs_clk_freq = data_rate * 1000 / 2;

	ddl_cntr_ref_reg = 5 * hs_clk_freq / 1000 / 2 / 16;

	if (data_rate >= 4500) {
		sel_fast = 1;
		max_phase = 63;
		dll_fbk = 7;
		coarse_bank = 0;
	} else if (data_rate >= 4000) {
		sel_fast = 1;
		max_phase = 71;
		dll_fbk = 8;
		coarse_bank = 1;
	} else if (data_rate >= 3600) {
		sel_fast = 1;
		max_phase = 79;
		dll_fbk = 9;
		coarse_bank = 1;
	} else if (data_rate >= 3230) {
		sel_fast = 1;
		max_phase = 87;
		dll_fbk = 10;
		coarse_bank = 1;
	} else if (data_rate >= 3000) {
		sel_fast = 0;
		max_phase = 71;
		dll_fbk = 8;
		coarse_bank = 0;
	} else if (data_rate >= 2700) {
		sel_fast = 0;
		max_phase = 79;
		dll_fbk = 9;
		coarse_bank = 1;
	} else if (data_rate >= 2455) {
		sel_fast = 0;
		max_phase = 87;
		dll_fbk = 10;
		coarse_bank = 1;
	} else if (data_rate >= 2250) {
		sel_fast = 0;
		max_phase = 95;
		dll_fbk = 11;
		coarse_bank = 1;
	} else if (data_rate >= 2077) {
		sel_fast = 0;
		max_phase = 103;
		dll_fbk = 12;
		coarse_bank = 1;
	} else if (data_rate >= 1929) {
		sel_fast = 0;
		max_phase = 111;
		dll_fbk = 13;
		coarse_bank = 2;
	} else if (data_rate >= 1800) {
		sel_fast = 0;
		max_phase = 119;
		dll_fbk = 14;
		coarse_bank = 2;
	} else if (data_rate >= 1688) {
		sel_fast = 0;
		max_phase = 127;
		dll_fbk = 15;
		coarse_bank = 2;
	} else if (data_rate >= 1588) {
		sel_fast = 0;
		max_phase = 135;
		dll_fbk = 16;
		coarse_bank = 2;
	} else if (data_rate >= 1500) {
		sel_fast = 0;
		max_phase = 143;
		dll_fbk = 17;
		coarse_bank = 3;
	} else {
		sel_fast = 0;
		max_phase = 0;
		dll_fbk = 0;
		coarse_bank = 0;
	}

	if (hs_clk_freq * 2 < 160000)
		hsrx_clk_div = 1;
	else if (hs_clk_freq * 2 < 320000)
		hsrx_clk_div = 2;
	else if (hs_clk_freq * 2 < 640000)
		hsrx_clk_div = 3;
	else if (hs_clk_freq * 2 < 1280000)
		hsrx_clk_div = 4;
	else if (hs_clk_freq * 2 < 2560000)
		hsrx_clk_div = 5;
	else
		hsrx_clk_div = 6;

	hs_thssettle = 115 + 1000 * 6 / data_rate;
	hs_thssettle = (hs_thssettle - T_DCO - (itminrx + 3) * T_DCO - 2 * T_DCO) / T_DCO - 1;
	fjump_deskew_reg = max_phase / 10 / 4;
	eye_open_deskew_reg = max_phase * 4 / 10 / 2;

	DDPMSG("data_rate=%d, ddl_cntr_ref_reg=%d, hs_thssettle=%d, fjump_deskew_reg=%d\n",
		data_rate, ddl_cntr_ref_reg, hs_thssettle, fjump_deskew_reg);

	if (hs_clk_freq * 2 >= 900000)
		cdr_coarse_trgt_reg = timebase * hs_clk_freq * 2 * 2 / 32 / 1000000 - 1;
	else
		cdr_coarse_trgt_reg = timebase * 900000 * 2 / 32 / 1000000 - 1;

	en_dly_deass_thresh_reg = 1000000 * 7 * 6 / hs_clk_freq / 2 / T_DCO +
		des_div_en_dly_deass_th;

	post_rcvd_rst_val = 2 * T_DCO * hs_clk_freq * 2 / 7 / 1000000 - 1;
	post_rcvd_rst_reg = (post_rcvd_rst_val > 0) ? post_rcvd_rst_val : 1;

	post_det_dly_thresh_val = ((189 * 1000000 / hs_clk_freq / 2) -
		(9 * 7 * 1000000 / hs_clk_freq / 2)) / T_DCO - 7;
	post_det_dly_thresh_reg = (post_det_dly_thresh_val > 0) ?
		post_det_dly_thresh_val : 1;

	DDPMSG("cdr_coarse_trgt_reg=%d, post_rcvd_rst_val=%d, fjump_deskew_reg=%d\n",
		cdr_coarse_trgt_reg, post_rcvd_rst_val, post_rcvd_rst_reg);
	DDPMSG(
		"en_dly_deass_thresh_reg=%d, post_det_dly_thresh_val=%d, post_det_dly_thresh_reg=%d\n",
		en_dly_deass_thresh_reg, post_det_dly_thresh_val, post_det_dly_thresh_reg);
}

void mipi_rx_enable(void *cmdq)
{
	BDG_OUTREG32(cmdq, OCLA_REG->OCLA_LANEC_CON, 0);
	BDG_OUTREG32(cmdq, OCLA_REG->OCLA_LANE0_CON, 0);
	BDG_OUTREG32(cmdq, OCLA_REG->OCLA_LANE1_CON, 0);
	BDG_OUTREG32(cmdq, OCLA_REG->OCLA_LANE2_CON, 0);
	BDG_OUTREG32(cmdq, OCLA_REG->OCLA_LANE3_CON, 0);
}

void mipi_rx_unset_forcerxmode(void *cmdq)
{

	BDG_OUTREGBIT(cmdq, struct OCLA_LANE_CON_REG,
			OCLA_REG->OCLA_LANEC_CON, FORCE_RX_MODE, 0);
	BDG_OUTREGBIT(cmdq, struct OCLA_LANE_CON_REG,
			OCLA_REG->OCLA_LANE0_CON, FORCE_RX_MODE, 0);
	BDG_OUTREGBIT(cmdq, struct OCLA_LANE_CON_REG,
			OCLA_REG->OCLA_LANE1_CON, FORCE_RX_MODE, 0);
	BDG_OUTREGBIT(cmdq, struct OCLA_LANE_CON_REG,
			OCLA_REG->OCLA_LANE2_CON, FORCE_RX_MODE, 0);
	BDG_OUTREGBIT(cmdq, struct OCLA_LANE_CON_REG,
			OCLA_REG->OCLA_LANE3_CON, FORCE_RX_MODE, 0);
}

void mipi_rx_set_forcerxmode(void *cmdq)
{
	BDG_OUTREGBIT(cmdq, struct OCLA_LANE_CON_REG,
			OCLA_REG->OCLA_LANEC_CON, FORCE_RX_MODE, 1);
	BDG_OUTREGBIT(cmdq, struct OCLA_LANE_CON_REG,
			OCLA_REG->OCLA_LANE0_CON, FORCE_RX_MODE, 1);
	BDG_OUTREGBIT(cmdq, struct OCLA_LANE_CON_REG,
			OCLA_REG->OCLA_LANE1_CON, FORCE_RX_MODE, 1);
	BDG_OUTREGBIT(cmdq, struct OCLA_LANE_CON_REG,
			OCLA_REG->OCLA_LANE2_CON, FORCE_RX_MODE, 1);
	BDG_OUTREGBIT(cmdq, struct OCLA_LANE_CON_REG,
			OCLA_REG->OCLA_LANE3_CON, FORCE_RX_MODE, 1);
}

void startup_seq_common(void *cmdq)
{

	BDG_OUTREG32(cmdq, OCLA_REG->OCLA_LANE_SWAP, 0x30213102);
	mipi_rx_enable(cmdq);
	mipi_rx_set_forcerxmode(cmdq);

	mtk_spi_mask_field_write(MIPI_RX_PHY_BASE + CORE_DIG_ANACTRL_RW_COMMON_ANACTRL_0 * 4,
		CORE_DIG_ANACTRL_RW_COMMON_ANACTRL_0_CB_LP_DCO_EN_DLY_MASK, 63);

	mtk_spi_mask_field_write(MIPI_RX_PHY_BASE + PPI_STARTUP_RW_COMMON_STARTUP_1_1 * 4,
		PPI_STARTUP_RW_COMMON_STARTUP_1_1_PHY_READY_DLY_MASK, 563);

	mtk_spi_mask_field_write(MIPI_RX_PHY_BASE + PPI_STARTUP_RW_COMMON_DPHY_6 * 4,
		PPI_STARTUP_RW_COMMON_DPHY_6_LP_DCO_CAL_addr_MASK, 39);

	mtk_spi_mask_field_write(MIPI_RX_PHY_BASE + PPI_CALIBCTRL_RW_COMMON_BG_0 * 4,
		PPI_CALIBCTRL_RW_COMMON_BG_0_BG_MAX_COUNTER_MASK, 500);

	mtk_spi_mask_field_write(MIPI_RX_PHY_BASE + PPI_RW_TERMCAL_CFG_0 * 4,
		PPI_RW_TERMCAL_CFG_0_TERMCAL_TIMER_MASK,
		25); // cfg_clk = 26 MHz

	mtk_spi_mask_field_write(MIPI_RX_PHY_BASE + PPI_RW_OFFSETCAL_CFG_0 * 4,
		PPI_RW_OFFSETCAL_CFG_0_OFFSETCAL_WAIT_THRESH_MASK,
		5); // cfg_clk = 26 MHz

	mtk_spi_mask_field_write(MIPI_RX_PHY_BASE + PPI_RW_LPDCOCAL_TIMEBASE * 4,
		PPI_RW_LPDCOCAL_TIMEBASE_LPCDCOCAL_TIMEBASE_MASK,
		103); // cfg_clk = 26 MHz

	mtk_spi_mask_field_write(MIPI_RX_PHY_BASE + PPI_RW_LPDCOCAL_NREF * 4,
		PPI_RW_LPDCOCAL_NREF_LPDCOCAL_NREF_MASK, 800);

	mtk_spi_mask_field_write(MIPI_RX_PHY_BASE + PPI_RW_LPDCOCAL_NREF_RANGE * 4,
		PPI_RW_LPDCOCAL_NREF_RANGE_LPDCOCAL_NREF_RANGE_MASK, 15);

	mtk_spi_mask_field_write(MIPI_RX_PHY_BASE + PPI_RW_LPDCOCAL_TWAIT_CONFIG * 4,
		PPI_RW_LPDCOCAL_TWAIT_CONFIG_LPDCOCAL_TWAIT_PON_MASK, 127);

	mtk_spi_mask_field_write(MIPI_RX_PHY_BASE + PPI_RW_LPDCOCAL_TWAIT_CONFIG * 4,
		PPI_RW_LPDCOCAL_TWAIT_CONFIG_LPDCOCAL_TWAIT_COARSE_MASK,
		32); // cfg_clk = 26 MHz

	mtk_spi_mask_field_write(MIPI_RX_PHY_BASE + PPI_RW_LPDCOCAL_VT_CONFIG * 4,
		PPI_RW_LPDCOCAL_VT_CONFIG_LPDCOCAL_TWAIT_FINE_MASK,
		32); // cfg_clk = 26 MHz

	mtk_spi_mask_field_write(MIPI_RX_PHY_BASE + PPI_RW_LPDCOCAL_VT_CONFIG * 4,
		PPI_RW_LPDCOCAL_VT_CONFIG_LPCDCOCAL_VT_NREF_RANGE_MASK, 15);

	mtk_spi_mask_field_write(MIPI_RX_PHY_BASE + PPI_RW_LPDCOCAL_VT_CONFIG * 4,
		PPI_RW_LPDCOCAL_VT_CONFIG_LPCDCOCAL_USE_IDEAL_NREF_MASK, 0);

	mtk_spi_mask_field_write(MIPI_RX_PHY_BASE + PPI_RW_LPDCOCAL_VT_CONFIG * 4,
		PPI_RW_LPDCOCAL_VT_CONFIG_LPCDCOCAL_VT_TRACKING_EN_MASK, 0);

	mtk_spi_mask_field_write(MIPI_RX_PHY_BASE + PPI_RW_LPDCOCAL_COARSE_CFG * 4,
		PPI_RW_LPDCOCAL_COARSE_CFG_NCOARSE_START_MASK, 1);

	mtk_spi_mask_field_write(MIPI_RX_PHY_BASE +
		CORE_DIG_IOCTRL_RW_AFE_CB_CTRL_2_6 * 4,
		CORE_DIG_IOCTRL_RW_AFE_CB_CTRL_2_6_OA_CB_HSTXLB_DCO_PON_OVR_EN_MASK,
		0);

	mtk_spi_mask_field_write(MIPI_RX_PHY_BASE + PPI_RW_COMMON_CFG * 4,
		PPI_RW_COMMON_CFG_CFG_CLK_DIV_FACTOR_MASK, 3);
}

int check_stopstate(void *cmdq)
{
	unsigned int timeout = 5000;
	unsigned int stop_state = 0, count = 0;

	while (timeout) {
		stop_state = mtk_spi_read((unsigned long)(&OCLA_REG->OCLA_LANE0_STOPSTATE));

		DDPMSG("stop_state=0x%x, timeout=%d\n", stop_state, timeout);

		if ((stop_state & 0x1) == 0x1)
			count++;

		if (count > 5)
			break;

		udelay(1);
		timeout--;
	}

	if (timeout == 0) {
		DDPMSG("%s, wait timeout!\n", __func__);
		return -1;
	}
	mipi_rx_unset_forcerxmode(cmdq);

	return 0;
}

int polling_status(void)
{
	unsigned int timeout = 5000;
	unsigned int status = 0;

	while (timeout) {
		status = mtk_spi_read((unsigned long)(&TX_REG[0]->DSI_TX_STATE_DBG7));
		if ((status & 0x800) == 0x800)
			break;

		udelay(1);
		timeout--;
	}

	if (timeout == 0) {
		DDPMSG("%s, wait timeout!\n", __func__);
		bdg_dsi_dump_reg(DISP_BDG_DSI0);
		return -1;
	}

	return 0;
}

int bdg_dsc_init(enum DISP_BDG_ENUM module,
			void *cmdq, struct mtk_dsi *dsi)
{
	unsigned int width = mtk_dsi_get_virtual_width(dsi, dsi->encoder.crtc);
	unsigned int height = mtk_dsi_get_virtual_heigh(dsi, dsi->encoder.crtc);
	unsigned int init_delay_limit, init_delay_height;
	unsigned int pic_group_width_m1;
	unsigned int pic_height_m1, pic_height_ext_m1, pic_height_ext_num;
	unsigned int slice_group_width_m1;
	unsigned int pad_num;
	unsigned int val;
	struct mtk_drm_crtc *mtk_crtc = dsi->ddp_comp.mtk_crtc;
	struct mtk_panel_dsc_params *params
		= &mtk_crtc->panel_ext->params->dsc_params;

	if (!dsc_en) {
		BDG_OUTREG32(cmdq, DSC_REG->DISP_DSC_PIC_H, height - 1);
		BDG_OUTREG32(cmdq, DSC_REG->DISP_DSC_PIC_W, width);
		return 0;
	}

	if (params->pic_width != width || params->pic_height != height) {
		DDPMSG("%s size mismatch...", __func__);
		return 1;
	}

	BDG_OUTREGBIT(cmdq, struct DISP_DSC_CON_REG,
					DSC_REG->DISP_DSC_CON, DSC_UFOE_SEL, 1);
	/* DSC Empty flag always high */
	BDG_OUTREGBIT(cmdq, struct DISP_DSC_CON_REG,
					DSC_REG->DISP_DSC_CON, DSC_EMPTY_FLAG_SEL, 1);
	/* DSC output buffer as FHD(plus) */
	BDG_OUTREG32(cmdq, DSC_REG->DISP_DSC_OBUF, 0x800002C2);

	init_delay_limit =
		((128 + (params->xmit_delay + 2) / 3) * 3 +
		params->slice_width - 1) / params->slice_width;
	init_delay_height =
		(init_delay_limit > 15) ? 15 : init_delay_limit;

	val = params->slice_mode + (init_delay_height << 8) + (1 << 16);
	BDG_OUTREG32(cmdq, DSC_REG->DISP_DSC_MODE, val);

	pic_group_width_m1 = (width + 2) / 3 - 1;
	BDG_OUTREG32(cmdq, DSC_REG->DISP_DSC_PIC_W,
		(pic_group_width_m1 << 16) + width);

	pic_height_m1 = height - 1;
	pic_height_ext_num = (height + params->slice_height - 1) /
	    params->slice_height;
	pic_height_ext_m1 = pic_height_ext_num * params->slice_height - 1;
	BDG_OUTREG32(cmdq, DSC_REG->DISP_DSC_PIC_H,
		(pic_height_ext_m1 << 16) + pic_height_m1);

	slice_group_width_m1 = (params->slice_width + 2) / 3 - 1;
	BDG_OUTREG32(cmdq, DSC_REG->DISP_DSC_SLICE_W,
		(slice_group_width_m1 << 16) + params->slice_width);

	BDG_OUTREG32(cmdq, DSC_REG->DISP_DSC_SLICE_H,
		((params->slice_width % 3) << 30) +
		((pic_height_ext_num - 1) << 16) + params->slice_height - 1);

	BDG_OUTREG32(cmdq,  DSC_REG->DISP_DSC_CHUNK_SIZE,
		params->chunk_size);
	BDG_OUTREG32(cmdq, DSC_REG->DISP_DSC_BUF_SIZE,
		params->chunk_size * params->slice_height);

	pad_num = (params->chunk_size + 2) / 3 * 3 - params->chunk_size;
	BDG_OUTREG32(cmdq, DSC_REG->DISP_DSC_PAD, pad_num);

	if (params->dsc_cfg)
		BDG_OUTREG32(cmdq, DSC_REG->DISP_DSC_CFG, params->dsc_cfg);
	else
		BDG_OUTREG32(cmdq, DSC_REG->DISP_DSC_CFG, 0x22);

	if ((params->ver & 0xf) == 2)
		BDG_OUTREGBIT(cmdq, struct DISP_DSC_SHADOW_REG,
				DSC_REG->DISP_DSC_SHADOW, DSC_VERSION_MINOR, 0x2);/* DSC V1.2 */
	else
		BDG_OUTREGBIT(cmdq, struct DISP_DSC_SHADOW_REG,
				DSC_REG->DISP_DSC_SHADOW, DSC_VERSION_MINOR, 0x1);/* DSC V1.1 */

	/* set PPS */
	val = params->dsc_line_buf_depth + (params->bit_per_channel << 4) +
		(params->bit_per_pixel << 8) + (params->rct_on << 18) +
		(params->bp_enable << 19);
	BDG_OUTREG32(cmdq, DSC_REG->DISP_DSC_PPS[0], val);

	val = (params->xmit_delay) + (params->dec_delay << 16);
	BDG_OUTREG32(cmdq, DSC_REG->DISP_DSC_PPS[1], val);

	val = (params->scale_value) + (params->increment_interval << 16);
	BDG_OUTREG32(cmdq, DSC_REG->DISP_DSC_PPS[2], val);

	val = (params->decrement_interval) + (params->line_bpg_offset << 16);
	BDG_OUTREG32(cmdq, DSC_REG->DISP_DSC_PPS[3], val);

	val = (params->nfl_bpg_offset) + (params->slice_bpg_offset << 16);
	BDG_OUTREG32(cmdq, DSC_REG->DISP_DSC_PPS[4], val);

	val = (params->initial_offset) + (params->final_offset << 16);
	BDG_OUTREG32(cmdq, DSC_REG->DISP_DSC_PPS[5], val);

	val = (params->flatness_minqp) + (params->flatness_maxqp << 8) +
		(params->rc_model_size << 16);
	BDG_OUTREG32(cmdq, DSC_REG->DISP_DSC_PPS[6], val);

	val = (params->rc_edge_factor) + (params->rc_quant_incr_limit0 << 8) +
		(params->rc_quant_incr_limit1 << 16) +
		(params->rc_tgt_offset_hi << 24) +
		(params->rc_tgt_offset_lo << 28);
	BDG_OUTREG32(cmdq, DSC_REG->DISP_DSC_PPS[7], val);

	BDG_OUTREG32(cmdq, DSC_REG->DISP_DSC_PPS[8], 0x382a1c0e);
	BDG_OUTREG32(cmdq, DSC_REG->DISP_DSC_PPS[9], 0x69625446);
	BDG_OUTREG32(cmdq, DSC_REG->DISP_DSC_PPS[10], 0x7b797770);
	BDG_OUTREG32(cmdq, DSC_REG->DISP_DSC_PPS[11], 0x7e7d);
	BDG_OUTREG32(cmdq, DSC_REG->DISP_DSC_PPS[12], 0x800880);
	BDG_OUTREG32(cmdq, DSC_REG->DISP_DSC_PPS[13], 0xf8c100a1);
	BDG_OUTREG32(cmdq, DSC_REG->DISP_DSC_PPS[14], 0xe8e3f0e3);
	BDG_OUTREG32(cmdq, DSC_REG->DISP_DSC_PPS[15], 0xe103e0e3);
	BDG_OUTREG32(cmdq, DSC_REG->DISP_DSC_PPS[16], 0xd943e123);
	BDG_OUTREG32(cmdq, DSC_REG->DISP_DSC_PPS[17], 0xd185d965);
	BDG_OUTREG32(cmdq, DSC_REG->DISP_DSC_PPS[18], 0xd1a7d1a5);
	BDG_OUTREG32(cmdq, DSC_REG->DISP_DSC_PPS[19], 0xd1ed);

	return 0;
}


int mipi_dsi_rx_mac_init(enum DISP_BDG_ENUM module,
			struct mtk_dsi *dsi, void *cmdq)
{
	int ret = 0;
	unsigned int lanes = 0, ipi_mode_qst, out_type;
	unsigned int temp, frame_width;
	unsigned int ipi_tx_delay_qst, t_ipi_tx_delay;
	unsigned int t_ppi_clk, t_ipi_clk, t_hact_ppi, t_hact_ipi;
	/*bit2: HSRX EoTp enable, bit1: LPTX EoTp enable, bit0: LPRX EoTp enable*/
	unsigned int eotp_cfg = 4;
	unsigned int timeout = 5000;
	unsigned int phy_ready = 0, count = 0;

	lanes = dsi->lanes > 0 ? (dsi->lanes - 1) : 0;
	ipi_mode_qst = (dsi->mode_flags & MIPI_DSI_MODE_VIDEO_SYNC_PULSE) ? 0 : 1;
	out_type = ((dsi->mode_flags & MIPI_DSI_MODE_VIDEO) == 0) ? 1 : 0;
	frame_width = mtk_dsi_get_virtual_width(dsi, dsi->encoder.crtc);

	BDG_OUTREG32(cmdq, SYS_REG->DISP_MISC0, 1);
	BDG_OUTREG32(cmdq, DSI2_REG->DSI2_DEVICE_N_LANES_OS, 3);
	BDG_OUTREG32(cmdq, DSI2_REG->DSI2_DEVICE_PHY_SHUTDOWNZ_OS, 1);
	BDG_OUTREG32(cmdq, DSI2_REG->DSI2_DEVICE_PHY_RSTZ_OS, 1);

	BDG_OUTREG32(cmdq, DSI2_REG->DSI2_DEVICE_SOFT_RSTN_OS, 0);
	udelay(1);
	BDG_OUTREG32(cmdq, DSI2_REG->DSI2_DEVICE_SOFT_RSTN_OS, 1);

	BDG_OUTREG32(cmdq, DSI2_REG->DSI2_DEVICE_VERSION_OS, 0);
	BDG_OUTREG32(cmdq, DSI2_REG->DSI2_DEVICE_N_LANES_OS, lanes);
	BDG_OUTREG32(cmdq, DSI2_REG->DSI2_DEVICE_INT_ST_MAIN_OS, 0);
	BDG_OUTREG32(cmdq, DSI2_REG->DSI2_DEVICE_EOTP_CFG_OS, eotp_cfg);
//	if (out_type)
		BDG_OUTREG32(cmdq, DSI2_REG->DSI2_DEVICE_CTRL_CFG_OS, 0);

	BDG_OUTREG32(cmdq, DSI2_REG->DSI2_DEVICE_FIFO_STATUS_OS, 0);
	BDG_OUTREG32(cmdq, DSI2_REG->DSI2_DEVICE_PHY_MODE_OS, dsi->ext->params->is_cphy);
	BDG_OUTREG32(cmdq, DSI2_REG->DSI2_DEVICE_PHY_TEST_CTRL0_OS, 0);
	BDG_OUTREG32(cmdq, DSI2_REG->DSI2_DEVICE_PHY_TEST_CTRL1_OS, 0);
	BDG_OUTREG32(cmdq, DSI2_REG->DSI2_DEVICE_PHY_DATA_STATUS_OS, 0);
//	if (out_type) {
	BDG_OUTREG32(cmdq, DSI2_REG->DSI2_DEVICE_LPTXRDY_TO_CNT_OS, 0);
	BDG_OUTREG32(cmdq, DSI2_REG->DSI2_DEVICE_LPTX_TO_CNT_OS, 0);
	BDG_OUTREG32(cmdq, DSI2_REG->DSI2_DEVICE_HSRX_TO_CNT_OS, 0);
//	}

	//Interrupt Registers
	BDG_OUTREG32(cmdq, DSI2_REG->DSI2_DEVICE_INT_ST_PHY_FATAL_OS, 0);
	BDG_OUTREG32(cmdq, DSI2_REG->DSI2_DEVICE_INT_MASK_N_PHY_FATAL_OS,
		0xffffffff);
	BDG_OUTREG32(cmdq, DSI2_REG->DSI2_DEVICE_INT_FORCE_PHY_FATAL_OS, 0);
	BDG_OUTREG32(cmdq, DSI2_REG->DSI2_DEVICE_INT_ST_PHY_OS, 0);
	BDG_OUTREG32(cmdq, DSI2_REG->DSI2_DEVICE_INT_MASK_N_PHY_OS,
		0xffffffff);
	BDG_OUTREG32(cmdq, DSI2_REG->DSI2_DEVICE_INT_FORCE_PHY_OS, 0);
	BDG_OUTREG32(cmdq, DSI2_REG->DSI2_DEVICE_INT_ST_DSI_FATAL_OS, 0);
	BDG_OUTREG32(cmdq, DSI2_REG->DSI2_DEVICE_INT_MASK_N_DSI_FATAL_OS,
		0xffffffff);
	BDG_OUTREG32(cmdq, DSI2_REG->DSI2_DEVICE_INT_FORCE_DSI_FATAL_OS, 0);
	BDG_OUTREG32(cmdq, DSI2_REG->DSI2_DEVICE_INT_ST_DSI_OS, 0);
	BDG_OUTREG32(cmdq, DSI2_REG->DSI2_DEVICE_INT_MASK_N_DSI_OS,
		0xffffffff);
	BDG_OUTREG32(cmdq, DSI2_REG->DSI2_DEVICE_INT_FORCE_DSI_OS, 0);

//	if (out_type) {
	BDG_OUTREG32(cmdq, DSI2_REG->DSI2_DEVICE_INT_ST_DDI_FATAL_OS, 0);
	BDG_OUTREG32(cmdq, DSI2_REG->DSI2_DEVICE_INT_MASK_N_DDI_FATAL_OS,
		0xffffffff);
	BDG_OUTREG32(cmdq, DSI2_REG->DSI2_DEVICE_INT_FORCE_DDI_FATAL_OS, 0);
	BDG_OUTREG32(cmdq, DSI2_REG->DSI2_DEVICE_INT_ST_DDI_OS, 0);
	BDG_OUTREG32(cmdq, DSI2_REG->DSI2_DEVICE_INT_MASK_N_DDI_OS,
		0xffffffff);
	BDG_OUTREG32(cmdq, DSI2_REG->DSI2_DEVICE_INT_FORCE_DDI_OS, 0);
//	}
	//video mode/ipi
//	if (!out_type) {
	BDG_OUTREG32(cmdq, DSI2_REG->DSI2_DEVICE_INT_ST_IPI_FATAL_OS, 0);
	BDG_OUTREG32(cmdq, DSI2_REG->DSI2_DEVICE_INT_MASK_N_IPI_FATAL_OS,
		0xffffffff);
	BDG_OUTREG32(cmdq, DSI2_REG->DSI2_DEVICE_INT_FORCE_IPI_FATAL_OS, 0);
//	}
	BDG_OUTREG32(cmdq, DSI2_REG->DSI2_DEVICE_INT_ST_FIFO_FATAL_OS, 0);
	BDG_OUTREG32(cmdq, DSI2_REG->DSI2_DEVICE_INT_MASK_N_FIFO_FATAL_OS,
		0xffffffff);
	BDG_OUTREG32(cmdq, DSI2_REG->DSI2_DEVICE_INT_FORCE_FIFO_FATAL_OS, 0);

//	if (out_type) {
	BDG_OUTREG32(cmdq, DSI2_REG->DSI2_DEVICE_INT_ST_ERR_RPT_OS, 0);
	BDG_OUTREG32(cmdq, DSI2_REG->DSI2_DEVICE_INT_MASK_N_ERR_RPT_OS,
		0xffffffff);
	BDG_OUTREG32(cmdq, DSI2_REG->DSI2_DEVICE_INT_FORCE_ERR_RPT_OS, 0);
//	}
	BDG_OUTREG32(cmdq, DSI2_REG->DSI2_DEVICE_INT_ST_RX_TRIGGERS_OS, 0);
	BDG_OUTREG32(cmdq, DSI2_REG->DSI2_DEVICE_INT_MASK_N_RX_TRIGGERS_OS,
		0xffffffff);
	BDG_OUTREG32(cmdq, DSI2_REG->DSI2_DEVICE_INT_FORCE_RX_TRIGGERS_OS, 0);

//	if (out_type) {
	BDG_OUTREG32(cmdq, DSI2_REG->DSI2_DEVICE_DDI_RDY_TO_CNT_OS, 0);
	BDG_OUTREG32(cmdq, DSI2_REG->DSI2_DEVICE_DDI_RESP_TO_CNT_OS, 0);
	BDG_OUTREG32(cmdq, DSI2_REG->DSI2_DEVICE_DDI_VALID_VC_CFG_OS, 0xf);
	BDG_OUTREG32(cmdq, DSI2_REG->DSI2_DEVICE_DDI_CLK_MGR_CFG_OS, 0x37);
//	}

	//video mode/ipi
//	if (!out_type) {
		BDG_OUTREG32(cmdq, DSI2_REG->DSI2_DEVICE_IPI_MODE_CFG_OS, 0);

		if (ipi_mode_qst)
			BDG_OUTREG32(cmdq, DSI2_REG->DSI2_DEVICE_IPI_MODE_CFG_OS, 1);

		t_ipi_clk  = 1000 / MM_CLK;
		//t_hact_ipi = frame_width * t_ipi_clk;
		t_hact_ipi = frame_width * 1000 / MM_CLK;
		if (dsi->ext->params->is_cphy) { //c-phy
			temp = 7000;
			t_ppi_clk = temp / ap_tx_data_rate;
			//t_hact_ppi = ((6 + frame_width * 3) / (n_lanes + 1) / 2) * t_ppi_clk;
			t_hact_ppi = ((6 + frame_width * 3) * temp / ap_tx_data_rate /
				(lanes + 1) / 2);
		} else {
			temp = 8000;
			t_ppi_clk  = temp / ap_tx_data_rate;
			//t_hact_ppi = ((6 + frame_width * 3) / (n_lanes + 1)) * t_ppi_clk;
			t_hact_ppi = ((6 + frame_width * 3) * temp / ap_tx_data_rate /
				(lanes + 1));
		}

		if (t_hact_ppi > t_hact_ipi)
			ipi_tx_delay_qst = ((t_hact_ppi - t_hact_ipi) * MM_CLK +
					20 * temp * MM_CLK / ap_tx_data_rate) / 1000 + 4;
		else
		//ipi_tx_delay_qst =  (20 * (temp * MM_CLK / tx_data_rate / 1000) + 4);
			ipi_tx_delay_qst =  20 * temp * MM_CLK /
				ap_tx_data_rate / 1000 + 4;

		DDPINFO("ap_tx_data_rate=%d, temp=%d, t_ppi_clk=%d, t_ipi_clk=%d\n",
			ap_tx_data_rate, temp, t_ppi_clk, t_ipi_clk);
		DDPINFO("t_hact_ppi=%d, t_hact_ipi=%d\n", t_hact_ppi, t_hact_ipi);

		//t_ipi_tx_delay = ipi_tx_delay_qst_i * t_ipi_clk;
		t_ipi_tx_delay = ipi_tx_delay_qst * 1000 / MM_CLK;

		DDPINFO("ipi_tx_delay_qst=%d, t_ipi_tx_delay=%d\n",
			ipi_tx_delay_qst, t_ipi_tx_delay);

		BDG_OUTREG32(cmdq, DSI2_REG->DSI2_DEVICE_IPI_VALID_VC_CFG_OS, 0);
		BDG_OUTREG32(cmdq, DSI2_REG->DSI2_DEVICE_IPI_TX_DELAY_OS, ipi_tx_delay_qst);
//	}

	while (timeout) {
		phy_ready = mtk_spi_read((unsigned long)(&OCLA_REG->OCLA_PHY_READY));
		DDPMSG("phy_ready=0x%x, timeout=%d\n", phy_ready, timeout);

		if ((phy_ready & 0x1) == 0x1)
			count++;

		if (count > 5)
			break;

		udelay(1);
		timeout--;
	}

	if (timeout == 0) {
		ret = -1;
		DDPMSG("%s, wait timeout!\n", __func__);
	}

	return ret;
}

void startup_seq_dphy_specific(unsigned int data_rate)
{
	DDPMSG("%s, data_rate=%d\n", __func__, data_rate);

	mtk_spi_mask_field_write(MIPI_RX_PHY_BASE + CORE_DIG_RW_COMMON_7 * 4,
		CORE_DIG_RW_COMMON_7_LANE0_HSRX_WORD_CLK_SEL_GATING_REG_MASK,
		0);

	mtk_spi_mask_field_write(MIPI_RX_PHY_BASE + CORE_DIG_RW_COMMON_7 * 4,
		CORE_DIG_RW_COMMON_7_LANE1_HSRX_WORD_CLK_SEL_GATING_REG_MASK,
		0);

	mtk_spi_mask_field_write(MIPI_RX_PHY_BASE + CORE_DIG_RW_COMMON_7 * 4,
		CORE_DIG_RW_COMMON_7_LANE2_HSRX_WORD_CLK_SEL_GATING_REG_MASK,
		0);

	mtk_spi_mask_field_write(MIPI_RX_PHY_BASE + CORE_DIG_RW_COMMON_7 * 4,
		CORE_DIG_RW_COMMON_7_LANE3_HSRX_WORD_CLK_SEL_GATING_REG_MASK,
		0);

	mtk_spi_mask_field_write(MIPI_RX_PHY_BASE + CORE_DIG_RW_COMMON_7 * 4,
		CORE_DIG_RW_COMMON_7_LANE4_HSRX_WORD_CLK_SEL_GATING_REG_MASK,
		0);

	if (data_rate >= 1500)
		mtk_spi_mask_field_write(MIPI_RX_PHY_BASE +
		PPI_STARTUP_RW_COMMON_DPHY_7 * 4,
		PPI_STARTUP_RW_COMMON_DPHY_7_DPHY_DDL_CAL_addr_MASK,
		40);
	else
		mtk_spi_mask_field_write(MIPI_RX_PHY_BASE +
		PPI_STARTUP_RW_COMMON_DPHY_7 * 4,
		PPI_STARTUP_RW_COMMON_DPHY_7_DPHY_DDL_CAL_addr_MASK,
		104);

	mtk_spi_mask_field_write(MIPI_RX_PHY_BASE +
		PPI_STARTUP_RW_COMMON_DPHY_8 * 4,
		PPI_STARTUP_RW_COMMON_DPHY_8_CPHY_DDL_CAL_addr_MASK,
		80);

	mtk_spi_mask_field_write(MIPI_RX_PHY_BASE + PPI_RW_DDLCAL_CFG_0 * 4,
		PPI_RW_DDLCAL_CFG_0_DDLCAL_TIMEBASE_TARGET_MASK,
		125); // cfg_clk = 26 MHz

	mtk_spi_mask_field_write(MIPI_RX_PHY_BASE + PPI_RW_DDLCAL_CFG_7 * 4,
		PPI_RW_DDLCAL_CFG_7_DDLCAL_DECR_WAIT_MASK,
		34);

	mtk_spi_mask_field_write(MIPI_RX_PHY_BASE + PPI_RW_DDLCAL_CFG_1 * 4,
		PPI_RW_DDLCAL_CFG_1_DDLCAL_DISABLE_TIME_MASK,
		25);

	mtk_spi_mask_field_write(MIPI_RX_PHY_BASE + PPI_RW_DDLCAL_CFG_2 * 4,
		PPI_RW_DDLCAL_CFG_2_DDLCAL_WAIT_MASK,
		4);

	mtk_spi_mask_field_write(MIPI_RX_PHY_BASE + PPI_RW_DDLCAL_CFG_2 * 4,
		PPI_RW_DDLCAL_CFG_2_DDLCAL_TUNE_MODE_MASK,
		2);

	mtk_spi_mask_field_write(MIPI_RX_PHY_BASE + PPI_RW_DDLCAL_CFG_2 * 4,
		PPI_RW_DDLCAL_CFG_2_DDLCAL_DDL_DLL_MASK,
		1);

	mtk_spi_mask_field_write(MIPI_RX_PHY_BASE + PPI_RW_DDLCAL_CFG_2 * 4,
		PPI_RW_DDLCAL_CFG_2_DDLCAL_ENABLE_WAIT_MASK,
		25); // cfg_clk = 26 MHz

	mtk_spi_mask_field_write(MIPI_RX_PHY_BASE + PPI_RW_DDLCAL_CFG_2 * 4,
		PPI_RW_DDLCAL_CFG_2_DDLCAL_UPDATE_SETTINGS_MASK,
		1);

	mtk_spi_mask_field_write(MIPI_RX_PHY_BASE + PPI_RW_DDLCAL_CFG_4 * 4,
		PPI_RW_DDLCAL_CFG_4_DDLCAL_STUCK_THRESH_MASK,
		10);

	mtk_spi_mask_field_write(MIPI_RX_PHY_BASE + PPI_RW_DDLCAL_CFG_6 * 4,
		PPI_RW_DDLCAL_CFG_6_DDLCAL_MAX_DIFF_MASK,
		10);

	mtk_spi_mask_field_write(MIPI_RX_PHY_BASE + PPI_RW_DDLCAL_CFG_7 * 4,
		PPI_RW_DDLCAL_CFG_7_DDLCAL_START_DELAY_MASK,
		12); // cfg_clk = 26 MHz

	if (data_rate > 1500) {
		mtk_spi_mask_field_write(MIPI_RX_PHY_BASE + PPI_RW_DDLCAL_CFG_3 * 4,
			PPI_RW_DDLCAL_CFG_3_DDLCAL_COUNTER_REF_MASK,
			ddl_cntr_ref_reg);

		mtk_spi_mask_field_write(MIPI_RX_PHY_BASE + PPI_RW_DDLCAL_CFG_1 * 4,
			PPI_RW_DDLCAL_CFG_1_DDLCAL_MAX_PHASE_MASK,
			max_phase);

		mtk_spi_mask_field_write(MIPI_RX_PHY_BASE + PPI_RW_DDLCAL_CFG_5 * 4,
			PPI_RW_DDLCAL_CFG_5_DDLCAL_DLL_FBK_MASK,
			dll_fbk);

		mtk_spi_mask_field_write(MIPI_RX_PHY_BASE + PPI_RW_DDLCAL_CFG_5 * 4,
			PPI_RW_DDLCAL_CFG_5_DDLCAL_DDL_COARSE_BANK_MASK,
			coarse_bank);
	}

	mtk_spi_mask_field_write(MIPI_RX_PHY_BASE +
		CORE_DIG_IOCTRL_RW_AFE_LANE0_CTRL_2_8 * 4,
		CORE_DIG_IOCTRL_RW_AFE_LANE0_CTRL_2_8_OA_LANE0_HSRX_CDPHY_SEL_FAST_MASK,
		sel_fast);

	mtk_spi_mask_field_write(MIPI_RX_PHY_BASE +
		CORE_DIG_IOCTRL_RW_AFE_LANE1_CTRL_2_8 * 4,
		CORE_DIG_IOCTRL_RW_AFE_LANE1_CTRL_2_8_OA_LANE1_HSRX_CDPHY_SEL_FAST_MASK,
		sel_fast);

	mtk_spi_mask_field_write(MIPI_RX_PHY_BASE +
		CORE_DIG_IOCTRL_RW_AFE_LANE2_CTRL_2_8 * 4,
		CORE_DIG_IOCTRL_RW_AFE_LANE2_CTRL_2_8_OA_LANE2_HSRX_CDPHY_SEL_FAST_MASK,
		sel_fast);

	mtk_spi_mask_field_write(MIPI_RX_PHY_BASE +
		CORE_DIG_IOCTRL_RW_AFE_LANE3_CTRL_2_8 * 4,
		CORE_DIG_IOCTRL_RW_AFE_LANE3_CTRL_2_8_OA_LANE3_HSRX_CDPHY_SEL_FAST_MASK,
		sel_fast);

	mtk_spi_mask_field_write(MIPI_RX_PHY_BASE +
		CORE_DIG_IOCTRL_RW_AFE_LANE4_CTRL_2_8 * 4,
		CORE_DIG_IOCTRL_RW_AFE_LANE4_CTRL_2_8_OA_LANE4_HSRX_CDPHY_SEL_FAST_MASK,
		sel_fast);

	mtk_spi_mask_field_write(MIPI_RX_PHY_BASE + CORE_DIG_DLANE_0_RW_LP_0 * 4,
		CORE_DIG_DLANE_0_RW_LP_0_LP_0_TTAGO_REG_MASK,
		6);

	mtk_spi_mask_field_write(MIPI_RX_PHY_BASE + CORE_DIG_DLANE_1_RW_LP_0 * 4,
		CORE_DIG_DLANE_1_RW_LP_0_LP_0_TTAGO_REG_MASK,
		6);

	mtk_spi_mask_field_write(MIPI_RX_PHY_BASE + CORE_DIG_DLANE_2_RW_LP_0 * 4,
		CORE_DIG_DLANE_2_RW_LP_0_LP_0_TTAGO_REG_MASK,
		6);

	mtk_spi_mask_field_write(MIPI_RX_PHY_BASE + CORE_DIG_DLANE_3_RW_LP_0 * 4,
		CORE_DIG_DLANE_3_RW_LP_0_LP_0_TTAGO_REG_MASK,
		6);

	mtk_spi_mask_field_write(MIPI_RX_PHY_BASE +
		CORE_DIG_IOCTRL_RW_AFE_LANE0_CTRL_2_2 * 4,
		CORE_DIG_IOCTRL_RW_AFE_LANE0_CTRL_2_2_OA_LANE0_SEL_LANE_CFG_MASK,
		0);

	mtk_spi_mask_field_write(MIPI_RX_PHY_BASE +
		CORE_DIG_IOCTRL_RW_AFE_LANE1_CTRL_2_2 * 4,
		CORE_DIG_IOCTRL_RW_AFE_LANE1_CTRL_2_2_OA_LANE1_SEL_LANE_CFG_MASK,
		0);

	mtk_spi_mask_field_write(MIPI_RX_PHY_BASE +
		CORE_DIG_IOCTRL_RW_AFE_LANE2_CTRL_2_2 * 4,
		CORE_DIG_IOCTRL_RW_AFE_LANE2_CTRL_2_2_OA_LANE2_SEL_LANE_CFG_MASK,
		1);

	mtk_spi_mask_field_write(MIPI_RX_PHY_BASE +
		CORE_DIG_IOCTRL_RW_AFE_LANE3_CTRL_2_2 * 4,
		CORE_DIG_IOCTRL_RW_AFE_LANE3_CTRL_2_2_OA_LANE3_SEL_LANE_CFG_MASK,
		0);

	mtk_spi_mask_field_write(MIPI_RX_PHY_BASE +
		CORE_DIG_IOCTRL_RW_AFE_LANE4_CTRL_2_2 * 4,
		CORE_DIG_IOCTRL_RW_AFE_LANE4_CTRL_2_2_OA_LANE4_SEL_LANE_CFG_MASK,
		0);

	mtk_spi_mask_field_write(MIPI_RX_PHY_BASE + CORE_DIG_RW_COMMON_6 * 4,
		CORE_DIG_RW_COMMON_6_DESERIALIZER_EN_DEASS_COUNT_THRESH_D_MASK,
		1);

	mtk_spi_mask_field_write(MIPI_RX_PHY_BASE + CORE_DIG_RW_COMMON_6 * 4,
		CORE_DIG_RW_COMMON_6_DESERIALIZER_DIV_EN_DELAY_THRESH_D_MASK,
		1);

	if (data_rate > 1500) {
		mtk_spi_mask_field_write(MIPI_RX_PHY_BASE +
		CORE_DIG_IOCTRL_RW_AFE_LANE0_CTRL_2_12 * 4,
		CORE_DIG_IOCTRL_RW_AFE_LANE0_CTRL_2_12_OA_L0_HSRX_DPHY_DDL_BYPASS_EN_OVR_VAL_MASK,
		0);

		mtk_spi_mask_field_write(MIPI_RX_PHY_BASE +
		CORE_DIG_IOCTRL_RW_AFE_LANE1_CTRL_2_12 * 4,
		CORE_DIG_IOCTRL_RW_AFE_LANE1_CTRL_2_12_OA_L1_HSRX_DPHY_DDL_BYPASS_EN_OVR_VAL_MASK,
		0);

		mtk_spi_mask_field_write(MIPI_RX_PHY_BASE +
		CORE_DIG_IOCTRL_RW_AFE_LANE2_CTRL_2_12 * 4,
		CORE_DIG_IOCTRL_RW_AFE_LANE2_CTRL_2_12_OA_L2_HSRX_DPHY_DDL_BYPASS_EN_OVR_VAL_MASK,
		0);

		mtk_spi_mask_field_write(MIPI_RX_PHY_BASE +
		CORE_DIG_IOCTRL_RW_AFE_LANE3_CTRL_2_12 * 4,
		CORE_DIG_IOCTRL_RW_AFE_LANE3_CTRL_2_12_OA_L3_HSRX_DPHY_DDL_BYPASS_EN_OVR_VAL_MASK,
		0);

		mtk_spi_mask_field_write(MIPI_RX_PHY_BASE +
		CORE_DIG_IOCTRL_RW_AFE_LANE4_CTRL_2_12 * 4,
		CORE_DIG_IOCTRL_RW_AFE_LANE4_CTRL_2_12_OA_L4_HSRX_DPHY_DDL_BYPASS_EN_OVR_VAL_MASK,
		0);

		mtk_spi_mask_field_write(MIPI_RX_PHY_BASE +
		CORE_DIG_IOCTRL_RW_AFE_LANE0_CTRL_2_13 * 4,
		CORE_DIG_IOCTRL_RW_AFE_LANE0_CTRL_2_13_OA_LANE0_HSRX_DPHY_DDL_BYPASS_EN_OVR_EN_MASK,
		0);

		mtk_spi_mask_field_write(MIPI_RX_PHY_BASE +
		CORE_DIG_IOCTRL_RW_AFE_LANE1_CTRL_2_13 * 4,
		CORE_DIG_IOCTRL_RW_AFE_LANE1_CTRL_2_13_OA_LANE1_HSRX_DPHY_DDL_BYPASS_EN_OVR_EN_MASK,
		0);

		mtk_spi_mask_field_write(MIPI_RX_PHY_BASE +
		CORE_DIG_IOCTRL_RW_AFE_LANE2_CTRL_2_13 * 4,
		CORE_DIG_IOCTRL_RW_AFE_LANE2_CTRL_2_13_OA_LANE2_HSRX_DPHY_DDL_BYPASS_EN_OVR_EN_MASK,
		0);

		mtk_spi_mask_field_write(MIPI_RX_PHY_BASE +
		CORE_DIG_IOCTRL_RW_AFE_LANE3_CTRL_2_13 * 4,
		CORE_DIG_IOCTRL_RW_AFE_LANE3_CTRL_2_13_OA_LANE3_HSRX_DPHY_DDL_BYPASS_EN_OVR_EN_MASK,
		0);

		mtk_spi_mask_field_write(MIPI_RX_PHY_BASE +
		CORE_DIG_IOCTRL_RW_AFE_LANE4_CTRL_2_13 * 4,
		CORE_DIG_IOCTRL_RW_AFE_LANE4_CTRL_2_13_OA_LANE4_HSRX_DPHY_DDL_BYPASS_EN_OVR_EN_MASK,
		0);
	} else {
		mtk_spi_mask_field_write(MIPI_RX_PHY_BASE +
		CORE_DIG_IOCTRL_RW_AFE_LANE0_CTRL_2_12 * 4,
		CORE_DIG_IOCTRL_RW_AFE_LANE0_CTRL_2_12_OA_L0_HSRX_DPHY_DDL_BYPASS_EN_OVR_VAL_MASK,
		1);

		mtk_spi_mask_field_write(MIPI_RX_PHY_BASE +
		CORE_DIG_IOCTRL_RW_AFE_LANE1_CTRL_2_12 * 4,
		CORE_DIG_IOCTRL_RW_AFE_LANE1_CTRL_2_12_OA_L1_HSRX_DPHY_DDL_BYPASS_EN_OVR_VAL_MASK,
		1);

		mtk_spi_mask_field_write(MIPI_RX_PHY_BASE +
		CORE_DIG_IOCTRL_RW_AFE_LANE2_CTRL_2_12 * 4,
		CORE_DIG_IOCTRL_RW_AFE_LANE2_CTRL_2_12_OA_L2_HSRX_DPHY_DDL_BYPASS_EN_OVR_VAL_MASK,
		1);

		mtk_spi_mask_field_write(MIPI_RX_PHY_BASE +
		CORE_DIG_IOCTRL_RW_AFE_LANE3_CTRL_2_12 * 4,
		CORE_DIG_IOCTRL_RW_AFE_LANE3_CTRL_2_12_OA_L3_HSRX_DPHY_DDL_BYPASS_EN_OVR_VAL_MASK,
		1);

		mtk_spi_mask_field_write(MIPI_RX_PHY_BASE +
		CORE_DIG_IOCTRL_RW_AFE_LANE4_CTRL_2_12 * 4,
		CORE_DIG_IOCTRL_RW_AFE_LANE4_CTRL_2_12_OA_L4_HSRX_DPHY_DDL_BYPASS_EN_OVR_VAL_MASK,
		1);

		mtk_spi_mask_field_write(MIPI_RX_PHY_BASE +
		CORE_DIG_IOCTRL_RW_AFE_LANE0_CTRL_2_13 * 4,
		CORE_DIG_IOCTRL_RW_AFE_LANE0_CTRL_2_13_OA_LANE0_HSRX_DPHY_DDL_BYPASS_EN_OVR_EN_MASK,
		1);

		mtk_spi_mask_field_write(MIPI_RX_PHY_BASE +
		CORE_DIG_IOCTRL_RW_AFE_LANE1_CTRL_2_13 * 4,
		CORE_DIG_IOCTRL_RW_AFE_LANE1_CTRL_2_13_OA_LANE1_HSRX_DPHY_DDL_BYPASS_EN_OVR_EN_MASK,
		1);

		mtk_spi_mask_field_write(MIPI_RX_PHY_BASE +
		CORE_DIG_IOCTRL_RW_AFE_LANE2_CTRL_2_13 * 4,
		CORE_DIG_IOCTRL_RW_AFE_LANE2_CTRL_2_13_OA_LANE2_HSRX_DPHY_DDL_BYPASS_EN_OVR_EN_MASK,
		1);

		mtk_spi_mask_field_write(MIPI_RX_PHY_BASE +
		CORE_DIG_IOCTRL_RW_AFE_LANE3_CTRL_2_13 * 4,
		CORE_DIG_IOCTRL_RW_AFE_LANE3_CTRL_2_13_OA_LANE3_HSRX_DPHY_DDL_BYPASS_EN_OVR_EN_MASK,
		1);

		mtk_spi_mask_field_write(MIPI_RX_PHY_BASE +
		CORE_DIG_IOCTRL_RW_AFE_LANE4_CTRL_2_13 * 4,
		CORE_DIG_IOCTRL_RW_AFE_LANE4_CTRL_2_13_OA_LANE4_HSRX_DPHY_DDL_BYPASS_EN_OVR_EN_MASK,
		1);
	}

	mtk_spi_mask_field_write(MIPI_RX_PHY_BASE +
		CORE_DIG_IOCTRL_RW_AFE_LANE2_CTRL_2_9 * 4,
		CORE_DIG_IOCTRL_RW_AFE_LANE2_CTRL_2_9_OA_LANE2_HSRX_HS_CLK_DIV_MASK,
		hsrx_clk_div);

	mtk_spi_mask_field_write(MIPI_RX_PHY_BASE +
		CORE_DIG_DLANE_CLK_RW_HS_RX_0 * 4,
		CORE_DIG_DLANE_CLK_RW_HS_RX_0_HS_RX_0_TCLKSETTLE_REG_MASK,
		28);

	mtk_spi_mask_field_write(MIPI_RX_PHY_BASE +
		CORE_DIG_DLANE_CLK_RW_HS_RX_7 * 4,
		CORE_DIG_DLANE_CLK_RW_HS_RX_7_HS_RX_7_TCLKMISS_REG_MASK,
		6);

	mtk_spi_mask_field_write(MIPI_RX_PHY_BASE +
		CORE_DIG_DLANE_0_RW_HS_RX_0 * 4,
		CORE_DIG_DLANE_0_RW_HS_RX_0_HS_RX_0_THSSETTLE_REG_MASK,
		hs_thssettle);

	mtk_spi_mask_field_write(MIPI_RX_PHY_BASE +
		CORE_DIG_DLANE_1_RW_HS_RX_0 * 4,
		CORE_DIG_DLANE_1_RW_HS_RX_0_HS_RX_0_THSSETTLE_REG_MASK,
		hs_thssettle);

	mtk_spi_mask_field_write(MIPI_RX_PHY_BASE +
		CORE_DIG_DLANE_2_RW_HS_RX_0 * 4,
		CORE_DIG_DLANE_2_RW_HS_RX_0_HS_RX_0_THSSETTLE_REG_MASK,
		hs_thssettle);

	mtk_spi_mask_field_write(MIPI_RX_PHY_BASE +
		CORE_DIG_DLANE_3_RW_HS_RX_0 * 4,
		CORE_DIG_DLANE_3_RW_HS_RX_0_HS_RX_0_THSSETTLE_REG_MASK,
		hs_thssettle);

	if (data_rate > 1500) {
		mtk_spi_mask_field_write(MIPI_RX_PHY_BASE +
			CORE_DIG_DLANE_0_RW_CFG_1 * 4,
			CORE_DIG_DLANE_0_RW_CFG_1_CFG_1_DESKEW_SUPPORTED_REG_MASK,
			1);

		mtk_spi_mask_field_write(MIPI_RX_PHY_BASE +
			CORE_DIG_DLANE_1_RW_CFG_1 * 4,
			CORE_DIG_DLANE_1_RW_CFG_1_CFG_1_DESKEW_SUPPORTED_REG_MASK,
			1);

		mtk_spi_mask_field_write(MIPI_RX_PHY_BASE +
			CORE_DIG_DLANE_2_RW_CFG_1 * 4,
			CORE_DIG_DLANE_2_RW_CFG_1_CFG_1_DESKEW_SUPPORTED_REG_MASK,
			1);

		mtk_spi_mask_field_write(MIPI_RX_PHY_BASE +
			CORE_DIG_DLANE_3_RW_CFG_1 * 4,
			CORE_DIG_DLANE_3_RW_CFG_1_CFG_1_DESKEW_SUPPORTED_REG_MASK,
			1);

		mtk_spi_mask_field_write(MIPI_RX_PHY_BASE +
			CORE_DIG_DLANE_0_RW_CFG_1 * 4,
			CORE_DIG_DLANE_0_RW_CFG_1_CFG_1_SOT_DETECTION_REG_MASK,
			0);

		mtk_spi_mask_field_write(MIPI_RX_PHY_BASE +
			CORE_DIG_DLANE_1_RW_CFG_1 * 4,
			CORE_DIG_DLANE_1_RW_CFG_1_CFG_1_SOT_DETECTION_REG_MASK,
			0);

		mtk_spi_mask_field_write(MIPI_RX_PHY_BASE +
			CORE_DIG_DLANE_2_RW_CFG_1 * 4,
			CORE_DIG_DLANE_2_RW_CFG_1_CFG_1_SOT_DETECTION_REG_MASK,
			0);

		mtk_spi_mask_field_write(MIPI_RX_PHY_BASE +
			CORE_DIG_DLANE_3_RW_CFG_1 * 4,
			CORE_DIG_DLANE_3_RW_CFG_1_CFG_1_SOT_DETECTION_REG_MASK,
			0);
	} else {
		mtk_spi_mask_field_write(MIPI_RX_PHY_BASE +
			CORE_DIG_DLANE_0_RW_CFG_1 * 4,
			CORE_DIG_DLANE_0_RW_CFG_1_CFG_1_DESKEW_SUPPORTED_REG_MASK,
			0);

		mtk_spi_mask_field_write(MIPI_RX_PHY_BASE +
			CORE_DIG_DLANE_1_RW_CFG_1 * 4,
			CORE_DIG_DLANE_1_RW_CFG_1_CFG_1_DESKEW_SUPPORTED_REG_MASK,
			0);

		mtk_spi_mask_field_write(MIPI_RX_PHY_BASE +
			CORE_DIG_DLANE_2_RW_CFG_1 * 4,
			CORE_DIG_DLANE_2_RW_CFG_1_CFG_1_DESKEW_SUPPORTED_REG_MASK,
			0);

		mtk_spi_mask_field_write(MIPI_RX_PHY_BASE +
			CORE_DIG_DLANE_3_RW_CFG_1 * 4,
			CORE_DIG_DLANE_3_RW_CFG_1_CFG_1_DESKEW_SUPPORTED_REG_MASK,
			0);

		mtk_spi_mask_field_write(MIPI_RX_PHY_BASE +
			CORE_DIG_DLANE_0_RW_CFG_1 * 4,
			CORE_DIG_DLANE_0_RW_CFG_1_CFG_1_SOT_DETECTION_REG_MASK,
			1);

		mtk_spi_mask_field_write(MIPI_RX_PHY_BASE +
			CORE_DIG_DLANE_1_RW_CFG_1 * 4,
			CORE_DIG_DLANE_1_RW_CFG_1_CFG_1_SOT_DETECTION_REG_MASK,
			1);

		mtk_spi_mask_field_write(MIPI_RX_PHY_BASE +
			CORE_DIG_DLANE_2_RW_CFG_1 * 4,
			CORE_DIG_DLANE_2_RW_CFG_1_CFG_1_SOT_DETECTION_REG_MASK,
			1);

		mtk_spi_mask_field_write(MIPI_RX_PHY_BASE +
			CORE_DIG_DLANE_3_RW_CFG_1 * 4,
			CORE_DIG_DLANE_3_RW_CFG_1_CFG_1_SOT_DETECTION_REG_MASK,
			1);
	}

	if (data_rate > 2500) {
		mtk_spi_mask_field_write(MIPI_RX_PHY_BASE +
			CORE_DIG_DLANE_0_RW_HS_RX_2 * 4,
			CORE_DIG_DLANE_0_RW_HS_RX_2_HS_RX_2_IGNORE_ALTERNCAL_REG_MASK,
			0);

		mtk_spi_mask_field_write(MIPI_RX_PHY_BASE +
			CORE_DIG_DLANE_1_RW_HS_RX_2 * 4,
			CORE_DIG_DLANE_1_RW_HS_RX_2_HS_RX_2_IGNORE_ALTERNCAL_REG_MASK,
			0);

		mtk_spi_mask_field_write(MIPI_RX_PHY_BASE +
			CORE_DIG_DLANE_2_RW_HS_RX_2 * 4,
			CORE_DIG_DLANE_2_RW_HS_RX_2_HS_RX_2_IGNORE_ALTERNCAL_REG_MASK,
			0);

		mtk_spi_mask_field_write(MIPI_RX_PHY_BASE +
			CORE_DIG_DLANE_3_RW_HS_RX_2 * 4,
			CORE_DIG_DLANE_3_RW_HS_RX_2_HS_RX_2_IGNORE_ALTERNCAL_REG_MASK,
			0);

		mtk_spi_mask_field_write(MIPI_RX_PHY_BASE +
			CORE_DIG_DLANE_CLK_RW_HS_RX_2 * 4,
			CORE_DIG_DLANE_CLK_RW_HS_RX_2_HS_RX_2_IGNORE_ALTERNCAL_REG_MASK,
			0);
	} else {
		mtk_spi_mask_field_write(MIPI_RX_PHY_BASE +
			CORE_DIG_DLANE_0_RW_HS_RX_2 * 4,
			CORE_DIG_DLANE_0_RW_HS_RX_2_HS_RX_2_IGNORE_ALTERNCAL_REG_MASK,
			1);

		mtk_spi_mask_field_write(MIPI_RX_PHY_BASE +
			CORE_DIG_DLANE_1_RW_HS_RX_2 * 4,
			CORE_DIG_DLANE_1_RW_HS_RX_2_HS_RX_2_IGNORE_ALTERNCAL_REG_MASK,
			1);

		mtk_spi_mask_field_write(MIPI_RX_PHY_BASE +
			CORE_DIG_DLANE_2_RW_HS_RX_2 * 4,
			CORE_DIG_DLANE_2_RW_HS_RX_2_HS_RX_2_IGNORE_ALTERNCAL_REG_MASK,
			1);

		mtk_spi_mask_field_write(MIPI_RX_PHY_BASE +
			CORE_DIG_DLANE_3_RW_HS_RX_2 * 4,
			CORE_DIG_DLANE_3_RW_HS_RX_2_HS_RX_2_IGNORE_ALTERNCAL_REG_MASK,
			1);

		mtk_spi_mask_field_write(MIPI_RX_PHY_BASE +
			CORE_DIG_DLANE_CLK_RW_HS_RX_2 * 4,
			CORE_DIG_DLANE_CLK_RW_HS_RX_2_HS_RX_2_IGNORE_ALTERNCAL_REG_MASK,
			1);
	}

	mtk_spi_mask_field_write(MIPI_RX_PHY_BASE +
		CORE_DIG_DLANE_0_RW_HS_RX_2 * 4,
		CORE_DIG_DLANE_0_RW_HS_RX_2_HS_RX_2_UPDATE_SETTINGS_DESKEW_REG_MASK,
		1);

	mtk_spi_mask_field_write(MIPI_RX_PHY_BASE +
		CORE_DIG_DLANE_1_RW_HS_RX_2 * 4,
		CORE_DIG_DLANE_1_RW_HS_RX_2_HS_RX_2_UPDATE_SETTINGS_DESKEW_REG_MASK,
		1);

	mtk_spi_mask_field_write(MIPI_RX_PHY_BASE +
		CORE_DIG_DLANE_2_RW_HS_RX_2 * 4,
		CORE_DIG_DLANE_2_RW_HS_RX_2_HS_RX_2_UPDATE_SETTINGS_DESKEW_REG_MASK,
		1);

	mtk_spi_mask_field_write(MIPI_RX_PHY_BASE +
		CORE_DIG_DLANE_3_RW_HS_RX_2 * 4,
		CORE_DIG_DLANE_3_RW_HS_RX_2_HS_RX_2_UPDATE_SETTINGS_DESKEW_REG_MASK,
		1);

	mtk_spi_mask_field_write(MIPI_RX_PHY_BASE +
		CORE_DIG_DLANE_CLK_RW_HS_RX_2 * 4,
		CORE_DIG_DLANE_CLK_RW_HS_RX_2_HS_RX_2_UPDATE_SETTINGS_DESKEW_REG_MASK,
		1);

	mtk_spi_mask_field_write(MIPI_RX_PHY_BASE +
		CORE_DIG_DLANE_0_RW_HS_RX_1 * 4,
		CORE_DIG_DLANE_0_RW_HS_RX_1_HS_RX_1_FILTER_SIZE_DESKEW_REG_MASK,
		16);

	mtk_spi_mask_field_write(MIPI_RX_PHY_BASE +
		CORE_DIG_DLANE_1_RW_HS_RX_1 * 4,
		CORE_DIG_DLANE_1_RW_HS_RX_1_HS_RX_1_FILTER_SIZE_DESKEW_REG_MASK,
		16);

	mtk_spi_mask_field_write(MIPI_RX_PHY_BASE +
		CORE_DIG_DLANE_2_RW_HS_RX_1 * 4,
		CORE_DIG_DLANE_2_RW_HS_RX_1_HS_RX_1_FILTER_SIZE_DESKEW_REG_MASK,
		16);

	mtk_spi_mask_field_write(MIPI_RX_PHY_BASE +
		CORE_DIG_DLANE_3_RW_HS_RX_1 * 4,
		CORE_DIG_DLANE_3_RW_HS_RX_1_HS_RX_1_FILTER_SIZE_DESKEW_REG_MASK,
		16);

	mtk_spi_mask_field_write(MIPI_RX_PHY_BASE +
		CORE_DIG_DLANE_0_RW_HS_RX_2 * 4,
		CORE_DIG_DLANE_0_RW_HS_RX_2_HS_RX_2_WINDOW_SIZE_DESKEW_REG_MASK,
		3);

	mtk_spi_mask_field_write(MIPI_RX_PHY_BASE +
		CORE_DIG_DLANE_1_RW_HS_RX_2 * 4,
		CORE_DIG_DLANE_1_RW_HS_RX_2_HS_RX_2_WINDOW_SIZE_DESKEW_REG_MASK,
		3);

	mtk_spi_mask_field_write(MIPI_RX_PHY_BASE +
		CORE_DIG_DLANE_2_RW_HS_RX_2 * 4,
		CORE_DIG_DLANE_2_RW_HS_RX_2_HS_RX_2_WINDOW_SIZE_DESKEW_REG_MASK,
		3);

	mtk_spi_mask_field_write(MIPI_RX_PHY_BASE +
		CORE_DIG_DLANE_3_RW_HS_RX_2 * 4,
		CORE_DIG_DLANE_3_RW_HS_RX_2_HS_RX_2_WINDOW_SIZE_DESKEW_REG_MASK,
		3);

	mtk_spi_mask_field_write(MIPI_RX_PHY_BASE +
		CORE_DIG_DLANE_CLK_RW_HS_RX_2 * 4,
		CORE_DIG_DLANE_CLK_RW_HS_RX_2_HS_RX_2_WINDOW_SIZE_DESKEW_REG_MASK,
		3);

	mtk_spi_mask_field_write(MIPI_RX_PHY_BASE +
		CORE_DIG_DLANE_0_RW_HS_RX_3 * 4,
		CORE_DIG_DLANE_0_RW_HS_RX_3_HS_RX_3_STEP_SIZE_DESKEW_REG_MASK,
		1);

	mtk_spi_mask_field_write(MIPI_RX_PHY_BASE +
		CORE_DIG_DLANE_1_RW_HS_RX_3 * 4,
		CORE_DIG_DLANE_1_RW_HS_RX_3_HS_RX_3_STEP_SIZE_DESKEW_REG_MASK,
		1);

	mtk_spi_mask_field_write(MIPI_RX_PHY_BASE +
		CORE_DIG_DLANE_2_RW_HS_RX_3 * 4,
		CORE_DIG_DLANE_2_RW_HS_RX_3_HS_RX_3_STEP_SIZE_DESKEW_REG_MASK,
		1);

	mtk_spi_mask_field_write(MIPI_RX_PHY_BASE +
		CORE_DIG_DLANE_3_RW_HS_RX_3 * 4,
		CORE_DIG_DLANE_3_RW_HS_RX_3_HS_RX_3_STEP_SIZE_DESKEW_REG_MASK,
		1);

	mtk_spi_mask_field_write(MIPI_RX_PHY_BASE +
		CORE_DIG_DLANE_0_RW_HS_RX_4 * 4,
		CORE_DIG_DLANE_0_RW_HS_RX_4_HS_RX_4_MAX_ITERATIONS_DESKEW_REG_MASK,
		150);

	mtk_spi_mask_field_write(MIPI_RX_PHY_BASE +
		CORE_DIG_DLANE_1_RW_HS_RX_4 * 4,
		CORE_DIG_DLANE_1_RW_HS_RX_4_HS_RX_4_MAX_ITERATIONS_DESKEW_REG_MASK,
		150);

	mtk_spi_mask_field_write(MIPI_RX_PHY_BASE +
		CORE_DIG_DLANE_2_RW_HS_RX_4 * 4,
		CORE_DIG_DLANE_2_RW_HS_RX_4_HS_RX_4_MAX_ITERATIONS_DESKEW_REG_MASK,
		150);

	mtk_spi_mask_field_write(MIPI_RX_PHY_BASE +
		CORE_DIG_DLANE_3_RW_HS_RX_4 * 4,
		CORE_DIG_DLANE_3_RW_HS_RX_4_HS_RX_4_MAX_ITERATIONS_DESKEW_REG_MASK,
		150);

	mtk_spi_mask_field_write(MIPI_RX_PHY_BASE +
		CORE_DIG_DLANE_0_RW_HS_RX_5 * 4,
		CORE_DIG_DLANE_0_RW_HS_RX_5_HS_RX_5_DDL_LEFT_INIT_REG_MASK,
		0);

	mtk_spi_mask_field_write(MIPI_RX_PHY_BASE +
		CORE_DIG_DLANE_1_RW_HS_RX_5 * 4,
		CORE_DIG_DLANE_1_RW_HS_RX_5_HS_RX_5_DDL_LEFT_INIT_REG_MASK,
		0);

	mtk_spi_mask_field_write(MIPI_RX_PHY_BASE +
		CORE_DIG_DLANE_2_RW_HS_RX_5 * 4,
		CORE_DIG_DLANE_2_RW_HS_RX_5_HS_RX_5_DDL_LEFT_INIT_REG_MASK,
		0);

	mtk_spi_mask_field_write(MIPI_RX_PHY_BASE +
		CORE_DIG_DLANE_3_RW_HS_RX_5 * 4,
		CORE_DIG_DLANE_3_RW_HS_RX_5_HS_RX_5_DDL_LEFT_INIT_REG_MASK,
		0);

	mtk_spi_mask_field_write(MIPI_RX_PHY_BASE +
		CORE_DIG_DLANE_0_RW_HS_RX_5 * 4,
		CORE_DIG_DLANE_0_RW_HS_RX_5_HS_RX_5_DDL_MID_INIT_REG_MASK,
		1);

	mtk_spi_mask_field_write(MIPI_RX_PHY_BASE +
		CORE_DIG_DLANE_1_RW_HS_RX_5 * 4,
		CORE_DIG_DLANE_1_RW_HS_RX_5_HS_RX_5_DDL_MID_INIT_REG_MASK,
		1);

	mtk_spi_mask_field_write(MIPI_RX_PHY_BASE +
		CORE_DIG_DLANE_2_RW_HS_RX_5 * 4,
		CORE_DIG_DLANE_2_RW_HS_RX_5_HS_RX_5_DDL_MID_INIT_REG_MASK,
		1);

	mtk_spi_mask_field_write(MIPI_RX_PHY_BASE +
		CORE_DIG_DLANE_3_RW_HS_RX_5 * 4,
		CORE_DIG_DLANE_3_RW_HS_RX_5_HS_RX_5_DDL_MID_INIT_REG_MASK,
		1);

	mtk_spi_mask_field_write(MIPI_RX_PHY_BASE +
		CORE_DIG_DLANE_0_RW_HS_RX_6 * 4,
		CORE_DIG_DLANE_0_RW_HS_RX_6_HS_RX_6_DDL_RIGHT_INIT_REG_MASK,
		2);

	mtk_spi_mask_field_write(MIPI_RX_PHY_BASE +
		CORE_DIG_DLANE_1_RW_HS_RX_6 * 4,
		CORE_DIG_DLANE_1_RW_HS_RX_6_HS_RX_6_DDL_RIGHT_INIT_REG_MASK,
		2);

	mtk_spi_mask_field_write(MIPI_RX_PHY_BASE +
		CORE_DIG_DLANE_2_RW_HS_RX_6 * 4,
		CORE_DIG_DLANE_2_RW_HS_RX_6_HS_RX_6_DDL_RIGHT_INIT_REG_MASK,
		2);

	mtk_spi_mask_field_write(MIPI_RX_PHY_BASE +
		CORE_DIG_DLANE_3_RW_HS_RX_6 * 4,
		CORE_DIG_DLANE_3_RW_HS_RX_6_HS_RX_6_DDL_RIGHT_INIT_REG_MASK,
		2);

	mtk_spi_mask_field_write(MIPI_RX_PHY_BASE +
		CORE_DIG_DLANE_0_RW_HS_RX_7 * 4,
		CORE_DIG_DLANE_0_RW_HS_RX_7_HS_RX_7_DESKEW_AUTO_ALGO_SEL_REG_MASK,
		1);

	mtk_spi_mask_field_write(MIPI_RX_PHY_BASE +
		CORE_DIG_DLANE_1_RW_HS_RX_7 * 4,
		CORE_DIG_DLANE_1_RW_HS_RX_7_HS_RX_7_DESKEW_AUTO_ALGO_SEL_REG_MASK,
		1);

	mtk_spi_mask_field_write(MIPI_RX_PHY_BASE +
		CORE_DIG_DLANE_2_RW_HS_RX_7 * 4,
		CORE_DIG_DLANE_2_RW_HS_RX_7_HS_RX_7_DESKEW_AUTO_ALGO_SEL_REG_MASK,
		1);

	mtk_spi_mask_field_write(MIPI_RX_PHY_BASE +
		CORE_DIG_DLANE_3_RW_HS_RX_7 * 4,
		CORE_DIG_DLANE_3_RW_HS_RX_7_HS_RX_7_DESKEW_AUTO_ALGO_SEL_REG_MASK,
		1);

	mtk_spi_mask_field_write(MIPI_RX_PHY_BASE +
		CORE_DIG_DLANE_0_RW_HS_RX_3 * 4,
		CORE_DIG_DLANE_0_RW_HS_RX_3_HS_RX_3_FJUMP_DESKEW_REG_MASK,
		fjump_deskew_reg);

	mtk_spi_mask_field_write(MIPI_RX_PHY_BASE +
		CORE_DIG_DLANE_1_RW_HS_RX_3 * 4,
		CORE_DIG_DLANE_1_RW_HS_RX_3_HS_RX_3_FJUMP_DESKEW_REG_MASK,
		fjump_deskew_reg);

	mtk_spi_mask_field_write(MIPI_RX_PHY_BASE +
		CORE_DIG_DLANE_2_RW_HS_RX_3 * 4,
		CORE_DIG_DLANE_2_RW_HS_RX_3_HS_RX_3_FJUMP_DESKEW_REG_MASK,
		fjump_deskew_reg);

	mtk_spi_mask_field_write(MIPI_RX_PHY_BASE +
		CORE_DIG_DLANE_3_RW_HS_RX_3 * 4,
		CORE_DIG_DLANE_3_RW_HS_RX_3_HS_RX_3_FJUMP_DESKEW_REG_MASK,
		fjump_deskew_reg);

	mtk_spi_mask_field_write(MIPI_RX_PHY_BASE +
		CORE_DIG_DLANE_0_RW_HS_RX_6 * 4,
		CORE_DIG_DLANE_0_RW_HS_RX_6_HS_RX_6_MIN_EYE_OPENING_DESKEW_REG_MASK,
		eye_open_deskew_reg);

	mtk_spi_mask_field_write(MIPI_RX_PHY_BASE +
		CORE_DIG_DLANE_1_RW_HS_RX_6 * 4,
		CORE_DIG_DLANE_1_RW_HS_RX_6_HS_RX_6_MIN_EYE_OPENING_DESKEW_REG_MASK,
		eye_open_deskew_reg);

	mtk_spi_mask_field_write(MIPI_RX_PHY_BASE +
		CORE_DIG_DLANE_2_RW_HS_RX_6 * 4,
		CORE_DIG_DLANE_2_RW_HS_RX_6_HS_RX_6_MIN_EYE_OPENING_DESKEW_REG_MASK,
		eye_open_deskew_reg);

	mtk_spi_mask_field_write(MIPI_RX_PHY_BASE +
		CORE_DIG_DLANE_3_RW_HS_RX_6 * 4,
		CORE_DIG_DLANE_3_RW_HS_RX_6_HS_RX_6_MIN_EYE_OPENING_DESKEW_REG_MASK,
		eye_open_deskew_reg);

	mtk_spi_write(MIPI_RX_PHY_BASE +
		CORE_DIG_COMMON_RW_DESKEW_FINE_MEM * 4, 0x0404);

	mtk_spi_write(MIPI_RX_PHY_BASE +
		CORE_DIG_COMMON_RW_DESKEW_FINE_MEM * 4, 0x040C);

	mtk_spi_write(MIPI_RX_PHY_BASE +
		CORE_DIG_COMMON_RW_DESKEW_FINE_MEM * 4, 0x0414);

	mtk_spi_write(MIPI_RX_PHY_BASE +
		CORE_DIG_COMMON_RW_DESKEW_FINE_MEM * 4, 0x041C);

	mtk_spi_write(MIPI_RX_PHY_BASE +
		CORE_DIG_COMMON_RW_DESKEW_FINE_MEM * 4, 0x0423);

	mtk_spi_write(MIPI_RX_PHY_BASE +
		CORE_DIG_COMMON_RW_DESKEW_FINE_MEM * 4,
		0x0429);

	mtk_spi_write(MIPI_RX_PHY_BASE +
		CORE_DIG_COMMON_RW_DESKEW_FINE_MEM * 4,
		0x0430);

	mtk_spi_write(MIPI_RX_PHY_BASE +
		CORE_DIG_COMMON_RW_DESKEW_FINE_MEM * 4,
		0x043A);

	mtk_spi_write(MIPI_RX_PHY_BASE +
		CORE_DIG_COMMON_RW_DESKEW_FINE_MEM * 4,
		0x0445);

	mtk_spi_write(MIPI_RX_PHY_BASE +
		CORE_DIG_COMMON_RW_DESKEW_FINE_MEM * 4,
		0x044A);

	mtk_spi_write(MIPI_RX_PHY_BASE +
		CORE_DIG_COMMON_RW_DESKEW_FINE_MEM * 4,
		0x0450);

	mtk_spi_write(MIPI_RX_PHY_BASE +
		CORE_DIG_COMMON_RW_DESKEW_FINE_MEM * 4,
		0x045A);

	mtk_spi_write(MIPI_RX_PHY_BASE +
		CORE_DIG_COMMON_RW_DESKEW_FINE_MEM * 4,
		0x0465);

	mtk_spi_write(MIPI_RX_PHY_BASE +
		CORE_DIG_COMMON_RW_DESKEW_FINE_MEM * 4,
		0x0469);

	mtk_spi_write(MIPI_RX_PHY_BASE + CORE_DIG_COMMON_RW_DESKEW_FINE_MEM * 4,
		0x0472);

	mtk_spi_write(MIPI_RX_PHY_BASE + CORE_DIG_COMMON_RW_DESKEW_FINE_MEM * 4,
		0x047A);

	mtk_spi_write(MIPI_RX_PHY_BASE + CORE_DIG_COMMON_RW_DESKEW_FINE_MEM * 4,
		0x0485);

	mtk_spi_write(MIPI_RX_PHY_BASE + CORE_DIG_COMMON_RW_DESKEW_FINE_MEM * 4,
		0x0489);

	mtk_spi_write(MIPI_RX_PHY_BASE + CORE_DIG_COMMON_RW_DESKEW_FINE_MEM * 4,
		0x0490);

	mtk_spi_write(MIPI_RX_PHY_BASE + CORE_DIG_COMMON_RW_DESKEW_FINE_MEM * 4,
		0x049A);

	mtk_spi_write(MIPI_RX_PHY_BASE + CORE_DIG_COMMON_RW_DESKEW_FINE_MEM * 4,
		0x04A4);

	mtk_spi_write(MIPI_RX_PHY_BASE + CORE_DIG_COMMON_RW_DESKEW_FINE_MEM * 4,
		0x04AC);

	mtk_spi_write(MIPI_RX_PHY_BASE + CORE_DIG_COMMON_RW_DESKEW_FINE_MEM * 4,
		0x04B4);

	mtk_spi_write(MIPI_RX_PHY_BASE + CORE_DIG_COMMON_RW_DESKEW_FINE_MEM * 4,
		0x04BC);

	mtk_spi_write(MIPI_RX_PHY_BASE + CORE_DIG_COMMON_RW_DESKEW_FINE_MEM * 4,
		0x04C4);

	mtk_spi_write(MIPI_RX_PHY_BASE + CORE_DIG_COMMON_RW_DESKEW_FINE_MEM * 4,
		0x04CC);

	mtk_spi_write(MIPI_RX_PHY_BASE + CORE_DIG_COMMON_RW_DESKEW_FINE_MEM * 4,
		0x04D4);

	mtk_spi_write(MIPI_RX_PHY_BASE + CORE_DIG_COMMON_RW_DESKEW_FINE_MEM * 4,
		0x04DC);

	mtk_spi_write(MIPI_RX_PHY_BASE + CORE_DIG_COMMON_RW_DESKEW_FINE_MEM * 4,
		0x04E4);

	mtk_spi_write(MIPI_RX_PHY_BASE + CORE_DIG_COMMON_RW_DESKEW_FINE_MEM * 4,
		0x04EC);

	mtk_spi_write(MIPI_RX_PHY_BASE + CORE_DIG_COMMON_RW_DESKEW_FINE_MEM * 4,
		0x04F4);

	mtk_spi_write(MIPI_RX_PHY_BASE + CORE_DIG_COMMON_RW_DESKEW_FINE_MEM * 4,
		0x04FC);

	mtk_spi_write(MIPI_RX_PHY_BASE + CORE_DIG_COMMON_RW_DESKEW_FINE_MEM * 4,
		0x0504);

	mtk_spi_write(MIPI_RX_PHY_BASE + CORE_DIG_COMMON_RW_DESKEW_FINE_MEM * 4,
		0x050C);

	mtk_spi_write(MIPI_RX_PHY_BASE + CORE_DIG_COMMON_RW_DESKEW_FINE_MEM * 4,
		0x0514);

	mtk_spi_write(MIPI_RX_PHY_BASE + CORE_DIG_COMMON_RW_DESKEW_FINE_MEM * 4,
		0x051C);

	mtk_spi_write(MIPI_RX_PHY_BASE + CORE_DIG_COMMON_RW_DESKEW_FINE_MEM * 4,
		0x0523);

	mtk_spi_write(MIPI_RX_PHY_BASE + CORE_DIG_COMMON_RW_DESKEW_FINE_MEM * 4,
		0x0529);

	mtk_spi_write(MIPI_RX_PHY_BASE + CORE_DIG_COMMON_RW_DESKEW_FINE_MEM * 4,
		0x0530);

	mtk_spi_write(MIPI_RX_PHY_BASE + CORE_DIG_COMMON_RW_DESKEW_FINE_MEM * 4,
		0x053A);

	mtk_spi_write(MIPI_RX_PHY_BASE + CORE_DIG_COMMON_RW_DESKEW_FINE_MEM * 4,
		0x0545);

	mtk_spi_write(MIPI_RX_PHY_BASE + CORE_DIG_COMMON_RW_DESKEW_FINE_MEM * 4,
		0x054A);

	mtk_spi_write(MIPI_RX_PHY_BASE + CORE_DIG_COMMON_RW_DESKEW_FINE_MEM * 4,
		0x0550);

	mtk_spi_write(MIPI_RX_PHY_BASE + CORE_DIG_COMMON_RW_DESKEW_FINE_MEM * 4,
		0x055A);

	mtk_spi_write(MIPI_RX_PHY_BASE + CORE_DIG_COMMON_RW_DESKEW_FINE_MEM * 4,
		0x0565);

	mtk_spi_write(MIPI_RX_PHY_BASE + CORE_DIG_COMMON_RW_DESKEW_FINE_MEM * 4,
		0x0569);

	mtk_spi_write(MIPI_RX_PHY_BASE + CORE_DIG_COMMON_RW_DESKEW_FINE_MEM * 4,
		0x0572);

	mtk_spi_write(MIPI_RX_PHY_BASE + CORE_DIG_COMMON_RW_DESKEW_FINE_MEM * 4,
		0x057A);

	mtk_spi_write(MIPI_RX_PHY_BASE + CORE_DIG_COMMON_RW_DESKEW_FINE_MEM * 4,
		0x0585);

	mtk_spi_write(MIPI_RX_PHY_BASE + CORE_DIG_COMMON_RW_DESKEW_FINE_MEM * 4,
		0x0589);

	mtk_spi_write(MIPI_RX_PHY_BASE + CORE_DIG_COMMON_RW_DESKEW_FINE_MEM * 4,
		0x0590);

	mtk_spi_write(MIPI_RX_PHY_BASE + CORE_DIG_COMMON_RW_DESKEW_FINE_MEM * 4,
		0x059A);

	mtk_spi_write(MIPI_RX_PHY_BASE + CORE_DIG_COMMON_RW_DESKEW_FINE_MEM * 4,
		0x05A4);

	mtk_spi_write(MIPI_RX_PHY_BASE + CORE_DIG_COMMON_RW_DESKEW_FINE_MEM * 4,
		0x05AC);

	mtk_spi_write(MIPI_RX_PHY_BASE + CORE_DIG_COMMON_RW_DESKEW_FINE_MEM * 4,
		0x05B4);

	mtk_spi_write(MIPI_RX_PHY_BASE + CORE_DIG_COMMON_RW_DESKEW_FINE_MEM * 4,
		0x05BC);

	mtk_spi_write(MIPI_RX_PHY_BASE + CORE_DIG_COMMON_RW_DESKEW_FINE_MEM * 4,
		0x05C4);

	mtk_spi_write(MIPI_RX_PHY_BASE + CORE_DIG_COMMON_RW_DESKEW_FINE_MEM * 4,
		0x05CC);

	mtk_spi_write(MIPI_RX_PHY_BASE + CORE_DIG_COMMON_RW_DESKEW_FINE_MEM * 4,
		0x05D4);

	mtk_spi_write(MIPI_RX_PHY_BASE + CORE_DIG_COMMON_RW_DESKEW_FINE_MEM * 4,
		0x05DC);

	mtk_spi_write(MIPI_RX_PHY_BASE + CORE_DIG_COMMON_RW_DESKEW_FINE_MEM * 4,
		0x05E4);

	mtk_spi_write(MIPI_RX_PHY_BASE + CORE_DIG_COMMON_RW_DESKEW_FINE_MEM * 4,
		0x05EC);

	mtk_spi_write(MIPI_RX_PHY_BASE + CORE_DIG_COMMON_RW_DESKEW_FINE_MEM * 4,
		0x05F4);

	mtk_spi_write(MIPI_RX_PHY_BASE + CORE_DIG_COMMON_RW_DESKEW_FINE_MEM * 4,
		0x05FC);

	mtk_spi_write(MIPI_RX_PHY_BASE + CORE_DIG_COMMON_RW_DESKEW_FINE_MEM * 4,
		0x0604);

	mtk_spi_write(MIPI_RX_PHY_BASE + CORE_DIG_COMMON_RW_DESKEW_FINE_MEM * 4,
		0x060C);

	mtk_spi_write(MIPI_RX_PHY_BASE + CORE_DIG_COMMON_RW_DESKEW_FINE_MEM * 4,
		0x0614);

	mtk_spi_write(MIPI_RX_PHY_BASE + CORE_DIG_COMMON_RW_DESKEW_FINE_MEM * 4,
		0x061C);

	mtk_spi_write(MIPI_RX_PHY_BASE + CORE_DIG_COMMON_RW_DESKEW_FINE_MEM * 4,
		0x0623);

	mtk_spi_write(MIPI_RX_PHY_BASE + CORE_DIG_COMMON_RW_DESKEW_FINE_MEM * 4,
		0x0629);

	mtk_spi_write(MIPI_RX_PHY_BASE + CORE_DIG_COMMON_RW_DESKEW_FINE_MEM * 4,
		0x0632);

	mtk_spi_write(MIPI_RX_PHY_BASE + CORE_DIG_COMMON_RW_DESKEW_FINE_MEM * 4,
		0x063A);

	mtk_spi_write(MIPI_RX_PHY_BASE + CORE_DIG_COMMON_RW_DESKEW_FINE_MEM * 4,
		0x0645);

	mtk_spi_write(MIPI_RX_PHY_BASE + CORE_DIG_COMMON_RW_DESKEW_FINE_MEM * 4,
		0x064A);

	mtk_spi_write(MIPI_RX_PHY_BASE + CORE_DIG_COMMON_RW_DESKEW_FINE_MEM * 4,
		0x0650);

	mtk_spi_write(MIPI_RX_PHY_BASE + CORE_DIG_COMMON_RW_DESKEW_FINE_MEM * 4,
		0x065A);

	mtk_spi_write(MIPI_RX_PHY_BASE + CORE_DIG_COMMON_RW_DESKEW_FINE_MEM * 4,
		0x0665);

	mtk_spi_write(MIPI_RX_PHY_BASE + CORE_DIG_COMMON_RW_DESKEW_FINE_MEM * 4,
		0x0669);

	mtk_spi_write(MIPI_RX_PHY_BASE + CORE_DIG_COMMON_RW_DESKEW_FINE_MEM * 4,
		0x0672);

	mtk_spi_write(MIPI_RX_PHY_BASE + CORE_DIG_COMMON_RW_DESKEW_FINE_MEM * 4,
		0x067A);

	mtk_spi_write(MIPI_RX_PHY_BASE + CORE_DIG_COMMON_RW_DESKEW_FINE_MEM * 4,
		0x0685);

	mtk_spi_write(MIPI_RX_PHY_BASE + CORE_DIG_COMMON_RW_DESKEW_FINE_MEM * 4,
		0x0689);

	mtk_spi_write(MIPI_RX_PHY_BASE + CORE_DIG_COMMON_RW_DESKEW_FINE_MEM * 4,
		0x0690);

	mtk_spi_write(MIPI_RX_PHY_BASE + CORE_DIG_COMMON_RW_DESKEW_FINE_MEM * 4,
		0x069A);

	mtk_spi_write(MIPI_RX_PHY_BASE + CORE_DIG_COMMON_RW_DESKEW_FINE_MEM * 4,
		0x06A4);

	mtk_spi_write(MIPI_RX_PHY_BASE + CORE_DIG_COMMON_RW_DESKEW_FINE_MEM * 4,
		0x06AC);

	mtk_spi_write(MIPI_RX_PHY_BASE + CORE_DIG_COMMON_RW_DESKEW_FINE_MEM * 4,
		0x06B4);

	mtk_spi_write(MIPI_RX_PHY_BASE + CORE_DIG_COMMON_RW_DESKEW_FINE_MEM * 4,
		0x06BC);

	mtk_spi_write(MIPI_RX_PHY_BASE + CORE_DIG_COMMON_RW_DESKEW_FINE_MEM * 4,
		0x06C4);

	mtk_spi_write(MIPI_RX_PHY_BASE + CORE_DIG_COMMON_RW_DESKEW_FINE_MEM * 4,
		0x06CC);

	mtk_spi_write(MIPI_RX_PHY_BASE + CORE_DIG_COMMON_RW_DESKEW_FINE_MEM * 4,
		0x06D4);

	mtk_spi_write(MIPI_RX_PHY_BASE + CORE_DIG_COMMON_RW_DESKEW_FINE_MEM * 4,
		0x06DC);

	mtk_spi_write(MIPI_RX_PHY_BASE + CORE_DIG_COMMON_RW_DESKEW_FINE_MEM * 4,
		0x06E4);

	mtk_spi_write(MIPI_RX_PHY_BASE + CORE_DIG_COMMON_RW_DESKEW_FINE_MEM * 4,
		0x06EC);

	mtk_spi_write(MIPI_RX_PHY_BASE + CORE_DIG_COMMON_RW_DESKEW_FINE_MEM * 4,
		0x06F4);

	mtk_spi_write(MIPI_RX_PHY_BASE + CORE_DIG_COMMON_RW_DESKEW_FINE_MEM * 4,
		0x06FC);

	mtk_spi_write(MIPI_RX_PHY_BASE + CORE_DIG_COMMON_RW_DESKEW_FINE_MEM * 4,
		0x0704);

	mtk_spi_write(MIPI_RX_PHY_BASE + CORE_DIG_COMMON_RW_DESKEW_FINE_MEM * 4,
		0x070C);

	mtk_spi_write(MIPI_RX_PHY_BASE + CORE_DIG_COMMON_RW_DESKEW_FINE_MEM * 4,
		0x0714);

	mtk_spi_write(MIPI_RX_PHY_BASE + CORE_DIG_COMMON_RW_DESKEW_FINE_MEM * 4,
		0x071C);

	mtk_spi_write(MIPI_RX_PHY_BASE + CORE_DIG_COMMON_RW_DESKEW_FINE_MEM * 4,
		0x0723);

	mtk_spi_write(MIPI_RX_PHY_BASE + CORE_DIG_COMMON_RW_DESKEW_FINE_MEM * 4,
		0x072A);

	mtk_spi_write(MIPI_RX_PHY_BASE + CORE_DIG_COMMON_RW_DESKEW_FINE_MEM * 4,
		0x0730);

	mtk_spi_write(MIPI_RX_PHY_BASE + CORE_DIG_COMMON_RW_DESKEW_FINE_MEM * 4,
		0x073A);

	mtk_spi_write(MIPI_RX_PHY_BASE + CORE_DIG_COMMON_RW_DESKEW_FINE_MEM * 4,
		0x0745);

	mtk_spi_write(MIPI_RX_PHY_BASE + CORE_DIG_COMMON_RW_DESKEW_FINE_MEM * 4,
		0x074A);

	mtk_spi_write(MIPI_RX_PHY_BASE + CORE_DIG_COMMON_RW_DESKEW_FINE_MEM * 4,
		0x0750);

	mtk_spi_write(MIPI_RX_PHY_BASE + CORE_DIG_COMMON_RW_DESKEW_FINE_MEM * 4,
		0x075A);

	mtk_spi_write(MIPI_RX_PHY_BASE + CORE_DIG_COMMON_RW_DESKEW_FINE_MEM * 4,
		0x0765);

	mtk_spi_write(MIPI_RX_PHY_BASE + CORE_DIG_COMMON_RW_DESKEW_FINE_MEM * 4,
		0x0769);

	mtk_spi_write(MIPI_RX_PHY_BASE + CORE_DIG_COMMON_RW_DESKEW_FINE_MEM * 4,
		0x0772);

	mtk_spi_write(MIPI_RX_PHY_BASE + CORE_DIG_COMMON_RW_DESKEW_FINE_MEM * 4,
		0x077A);

	mtk_spi_write(MIPI_RX_PHY_BASE + CORE_DIG_COMMON_RW_DESKEW_FINE_MEM * 4,
		0x0785);

	mtk_spi_write(MIPI_RX_PHY_BASE + CORE_DIG_COMMON_RW_DESKEW_FINE_MEM * 4,
		0x0789);

	mtk_spi_write(MIPI_RX_PHY_BASE + CORE_DIG_COMMON_RW_DESKEW_FINE_MEM * 4,
		0x0790);

	mtk_spi_write(MIPI_RX_PHY_BASE + CORE_DIG_COMMON_RW_DESKEW_FINE_MEM * 4,
		0x079A);

	mtk_spi_write(MIPI_RX_PHY_BASE + CORE_DIG_COMMON_RW_DESKEW_FINE_MEM * 4,
		0x07A4);

	mtk_spi_write(MIPI_RX_PHY_BASE + CORE_DIG_COMMON_RW_DESKEW_FINE_MEM * 4,
		0x07AC);

	mtk_spi_write(MIPI_RX_PHY_BASE + CORE_DIG_COMMON_RW_DESKEW_FINE_MEM * 4,
		0x07B4);

	mtk_spi_write(MIPI_RX_PHY_BASE + CORE_DIG_COMMON_RW_DESKEW_FINE_MEM * 4,
		0x07BC);

	mtk_spi_write(MIPI_RX_PHY_BASE + CORE_DIG_COMMON_RW_DESKEW_FINE_MEM * 4,
		0x07C4);

	mtk_spi_write(MIPI_RX_PHY_BASE + CORE_DIG_COMMON_RW_DESKEW_FINE_MEM * 4,
		0x07CC);

	mtk_spi_write(MIPI_RX_PHY_BASE + CORE_DIG_COMMON_RW_DESKEW_FINE_MEM * 4,
		0x07D4);

	mtk_spi_write(MIPI_RX_PHY_BASE + CORE_DIG_COMMON_RW_DESKEW_FINE_MEM * 4,
		0x07DC);

	mtk_spi_write(MIPI_RX_PHY_BASE + CORE_DIG_COMMON_RW_DESKEW_FINE_MEM * 4,
		0x07E4);

	mtk_spi_write(MIPI_RX_PHY_BASE + CORE_DIG_COMMON_RW_DESKEW_FINE_MEM * 4,
		0x07EC);

	mtk_spi_write(MIPI_RX_PHY_BASE + CORE_DIG_COMMON_RW_DESKEW_FINE_MEM * 4,
		0x07F4);

	mtk_spi_write(MIPI_RX_PHY_BASE + CORE_DIG_COMMON_RW_DESKEW_FINE_MEM * 4,
		0x07FC);

#ifdef _Disable_HS_DCO_
		mtk_spi_mask_field_write(MIPI_RX_PHY_BASE +
			CORE_DIG_IOCTRL_RW_AFE_CB_CTRL_2_6 * 4,
			CORE_DIG_IOCTRL_RW_AFE_CB_CTRL_2_6_OA_CB_HSTXLB_DCO_EN_OVR_EN_MASK,
			1);
		mtk_spi_mask_field_write(MIPI_RX_PHY_BASE +
			CORE_DIG_IOCTRL_RW_AFE_CB_CTRL_2_6 * 4,
			CORE_DIG_IOCTRL_RW_AFE_CB_CTRL_2_6_OA_CB_HSTXLB_DCO_PON_OVR_EN_MASK,
			1);
		mtk_spi_mask_field_write(MIPI_RX_PHY_BASE +
			CORE_DIG_IOCTRL_RW_AFE_CB_CTRL_2_6 * 4,
			CORE_DIG_IOCTRL_RW_AFE_CB_CTRL_2_6_OA_CB_LP_DCO_EN_OVR_VAL_MASK,
			0);
		mtk_spi_mask_field_write(MIPI_RX_PHY_BASE +
			CORE_DIG_IOCTRL_RW_AFE_CB_CTRL_2_6 * 4,
			CORE_DIG_IOCTRL_RW_AFE_CB_CTRL_2_6_OA_CB_LP_DCO_PON_OVR_VAL_MASK,
			0);
#endif
#ifdef _Disable_LP_TX_L023_
		mtk_spi_mask_field_write(MIPI_RX_PHY_BASE + 0x1028 * 4, 0x80, 1);
		mtk_spi_mask_field_write(MIPI_RX_PHY_BASE + 0x1028 * 4, 0x40, 1);
		mtk_spi_mask_field_write(MIPI_RX_PHY_BASE + 0x1428 * 4, 0x80, 1);
		mtk_spi_mask_field_write(MIPI_RX_PHY_BASE + 0x1428 * 4, 0x40, 1);
		mtk_spi_mask_field_write(MIPI_RX_PHY_BASE + 0x1628 * 4, 0x80, 1);
		mtk_spi_mask_field_write(MIPI_RX_PHY_BASE + 0x1628 * 4, 0x40, 1);
		mtk_spi_mask_field_write(MIPI_RX_PHY_BASE + 0x1828 * 4, 0x80, 1);
		mtk_spi_mask_field_write(MIPI_RX_PHY_BASE + 0x1828 * 4, 0x40, 1);
		mtk_spi_mask_field_write(MIPI_RX_PHY_BASE + 0x1027 * 4, 0xf, 0);
		mtk_spi_mask_field_write(MIPI_RX_PHY_BASE + 0x1427 * 4, 0xf, 0);
		mtk_spi_mask_field_write(MIPI_RX_PHY_BASE + 0x1627 * 4, 0xf, 0);
		mtk_spi_mask_field_write(MIPI_RX_PHY_BASE + 0x1827 * 4, 0xf, 0);
#endif
#ifdef _G_MODE_EN_
		mtk_spi_mask_field_write(MIPI_RX_PHY_BASE +
			CORE_DIG_IOCTRL_RW_AFE_LANE0_CTRL_2_9 * 4, 0x18, 0);
		mtk_spi_mask_field_write(MIPI_RX_PHY_BASE +
			CORE_DIG_IOCTRL_RW_AFE_LANE1_CTRL_2_9 * 4, 0x18, 0);
		mtk_spi_mask_field_write(MIPI_RX_PHY_BASE +
			CORE_DIG_IOCTRL_RW_AFE_LANE2_CTRL_2_9 * 4, 0x18, 0);
		mtk_spi_mask_field_write(MIPI_RX_PHY_BASE +
			CORE_DIG_IOCTRL_RW_AFE_LANE3_CTRL_2_9 * 4, 0x18, 0);
		mtk_spi_mask_field_write(MIPI_RX_PHY_BASE +
			CORE_DIG_IOCTRL_RW_AFE_LANE4_CTRL_2_9 * 4, 0x18, 0);
#endif

}

/* for debug use */
void output_debug_signal(void)
{
#ifdef BDG_PORTING_DBG
	//Mutex thread 0 remove mod_sof[1]
	mtk_spi_write(0x00025030, 0x0000001D);

	//Mutex thread 1 use IPI_VSYNC falling and mod_sof[1]
	mtk_spi_write(0x00025050, 0x00000002);
	mtk_spi_write(0x0002504C, 0x0000004a);
	mtk_spi_write(0x00025040, 0x00000001);

	//DSI DBG Setting
	mtk_spi_write(0x00021170, 0x00001001);

	//MM DBG Setting
	mtk_spi_write(0x00023300, 0x00000003);
	mtk_spi_write(0x000231a8, 0x00000021);

	//DBGSYS Setting
	mtk_spi_write(0x000076d0, 0x00000001);

	//GPIO Mode
	mtk_spi_write(0x00007300, 0x77701111);
#endif
	mtk_spi_write(0x00007310, 0x31111111);

}

void bdg_first_init(void)
{
	SYS_REG = (struct BDG_SYSREG_CTRL_REGS *)DISPSYS_BDG_SYSREG_CTRL_BASE;
	TX_REG[0] = (struct BDG_TX_REGS *)DISPSYS_BDG_TX_DSI0_BASE;

	// request eint irq
	bdg_request_eint_irq();

	/***** NFC SRCLKENAI0 Interrupt Handler +++ *****/
	nfc_request_eint_irq();
	/***** NFC SRCLKENAI0 Interrupt Handler --- *****/

	/* open 6382 dsi eint */
	BDG_OUTREGBIT(NULL, struct IRQ_MSK_CLR_SET_REG, SYS_REG->IRQ_MSK_CLR, REG_04, 1);
	BDG_OUTREGBIT(NULL, struct DSI_TX_INTEN_REG,
			TX_REG[0]->DSI_TX_INTEN, BUFFER_UNDERRUN_INT_EN,
			1);
}

int bdg_common_init(enum DISP_BDG_ENUM module,
			struct mtk_dsi *dsi, void *cmdq)
{
	int ret = 0;
	unsigned int vact = 0;
	unsigned int hact = 0;
	struct mtk_drm_crtc *mtk_crtc = dsi->ddp_comp.mtk_crtc;
	struct mtk_panel_dsc_params *params
		= &mtk_crtc->panel_ext->params->dsc_params;
	dsc_en = params->bdg_dsc_enable;

	DDPMSG("%s+\n", __func__);

	DISPSYS_REG = (struct BDG_DISPSYS_CONFIG_REGS *)DISPSYS_BDG_MMSYS_CONFIG_BASE;
	DSI2_REG = (struct BDG_MIPIDSI2_REGS *)DISPSYS_BDG_MIPIDSI2_DEVICE_BASE;
	SYS_REG = (struct BDG_SYSREG_CTRL_REGS *)DISPSYS_BDG_SYSREG_CTRL_BASE;
	RDMA_REG = (struct BDG_RDMA0_REGS *)DISPSYS_BDG_RDMA0_REGS_BASE;
	MUTEX_REG = (struct BDG_MUTEX_REGS *)DISPSYS_BDG_MUTEX_REGS_BASE;
	OCLA_REG = (struct BDG_OCLA_REGS *)DISPSYS_BDG_OCLA_BASE;
	TX_REG[0] = (struct BDG_TX_REGS *)DISPSYS_BDG_TX_DSI0_BASE;
	MIPI_TX_REG = (struct BDG_MIPI_TX_REGS *)DISPSYS_BDG_MIPI_TX_BASE;
	DSC_REG = (struct BDG_DISP_DSC_REGS *)DISPSYS_BDG_DISP_DSC_BASE;
	APMIXEDSYS = (struct BDG_APMIXEDSYS_REGS *)DISPSYS_BDG_APMIXEDSYS_BASE;
	TOPCKGEN = (struct BDG_TOPCKGEN_REGS *)DISPSYS_BDG_TOPCKGEN_BASE;
	GCE_REG = (struct BDG_GCE_REGS *)DISPSYS_BDG_GCE_BASE;
	EFUSE = (struct BDG_EFUSE_REGS *)DISPSYS_BDG_EFUSE_BASE;
	GPIO = (struct BDG_GPIO_REGS *)DISPSYS_BDG_GPIO_BASE;
	TX_CMDQ_REG[0] = (struct DSI_TX_CMDQ_REGS *)(DISPSYS_BDG_TX_DSI0_BASE + 0xd00);

	/* open 26m clk */
	clk_buf_disp_ctrl(true);
	bdg_tx_pull_6382_reset_pin(dsi);

	/* spi init & set low speed */
	spislv_init();
	spislv_switch_speed_hz(SPI_TX_LOW_SPEED_HZ, SPI_RX_LOW_SPEED_HZ);

	if (nfc_clk_already_enabled)
		bdg_clk_buf_nfc(1);
	else
		bdg_clk_buf_nfc(0);

	set_LDO_on(cmdq);
	set_mtcmos_on(cmdq);
	ana_macro_on(cmdq);
	set_subsys_on(cmdq);

	spislv_switch_speed_hz(SPI_TX_MAX_SPEED_HZ, SPI_RX_MAX_SPEED_HZ);
	// Disable reset sequential de-glitch circuit
	BDG_OUTREG32(cmdq, SYS_REG->RST_DG_CTRL, 0);
	// Set GPIO to active IRQ
	BDG_OUTREGBIT(cmdq, struct GPIO_MODE1_REG, GPIO->GPIO_MODE1, GPIO12, 1);
	/* open 6382 dsi eint */
	BDG_OUTREGBIT(NULL, struct IRQ_MSK_CLR_SET_REG, SYS_REG->IRQ_MSK_CLR, REG_04, 1);

	// for release rx mac reset
	BDG_OUTREG32(cmdq, SYS_REG->DISP_MISC0, 1);
	BDG_OUTREG32(cmdq, SYS_REG->DISP_MISC0, 0);
	// switch DDI(cmd mode) or IPI(vdo mode) path
	BDG_OUTREG32(cmdq, DISPSYS_REG->DDI_POST_CTRL, 0x00001100);

	/* set TE_OUT bypass, TE_IN form ddic */
	BDG_OUTREGBIT(cmdq, struct GPIO_MODE1_REG, GPIO->GPIO_MODE1, RSV_24, 0x31);
	if (dsi->mode_flags & MIPI_DSI_MODE_VIDEO)
		BDG_OUTREGBIT(cmdq, struct MIPI_RX_POST_CTRL_REG,
				DISPSYS_REG->MIPI_RX_POST_CTRL, MIPI_RX_MODE_SEL, 1);
	else
		BDG_OUTREGBIT(cmdq, struct MIPI_RX_POST_CTRL_REG,
				DISPSYS_REG->MIPI_RX_POST_CTRL, MIPI_RX_MODE_SEL, 0);

	// RDMA setting
	vact = mtk_dsi_get_virtual_heigh(dsi, dsi->encoder.crtc);
	hact = mtk_dsi_get_virtual_width(dsi, dsi->encoder.crtc);
	BDG_OUTREG32(cmdq, RDMA_REG->DISP_RDMA_SIZE_CON_0,
			hact);
	BDG_OUTREG32(cmdq, RDMA_REG->DISP_RDMA_SIZE_CON_1,
			vact);

	BDG_OUTREG32(cmdq, RDMA_REG->DISP_RDMA_FIFO_CON, 0xf30 << 16);
	BDG_OUTREG32(cmdq, RDMA_REG->DISP_RDMA_STALL_CG_CON, 0);
	BDG_OUTREGBIT(cmdq, struct DISP_RDMA_SRAM_SEL_REG,
			RDMA_REG->DISP_RDMA_SRAM_SEL, RDMA_SRAM_SEL, 0);
	BDG_OUTREGBIT(cmdq, struct DISP_RDMA_GLOBAL_CON_REG,
			RDMA_REG->DISP_RDMA_GLOBAL_CON, ENGINE_EN, 1);

	// MUTEX setting
	BDG_OUTREG32(cmdq, MUTEX_REG->DISP_MUTEX_INTEN, 0x1 << 16);
	BDG_OUTREG32(cmdq, MUTEX_REG->DISP_MUTEX0_MOD0, 0x1f);
	if ((dsi->mode_flags & MIPI_DSI_MODE_VIDEO) == 0)
		BDG_OUTREG32(cmdq, MUTEX_REG->DISP_MUTEX0_CTL, 0);
	else {
		BDG_OUTREG32(cmdq, MUTEX_REG->DISP_MUTEX0_CTL,
				0x1 << 0 | 0x0 << 3 | 0x1 << 6 | 0x0 << 9);
		BDG_OUTREGBIT(cmdq, struct DISP_MUTEX0_EN_REG,
			MUTEX_REG->DISP_MUTEX0_EN, MUTEX0_EN, 1);
	}

	// DSI-TX setting
	bdg_tx_init(module, dsi, NULL);
	BDG_OUTREG32(cmdq, TX_REG[0]->DSI_RESYNC_CON, 0x50007);

	// DSC setting
	if (dsc_en) {
		bdg_dsc_init(module, cmdq, dsi);
		BDG_OUTREGBIT(cmdq, struct DISP_DSC_CON_REG,
				DSC_REG->DISP_DSC_CON, DSC_PT_MEM_EN, 1);
		BDG_OUTREGBIT(cmdq, struct DISP_DSC_CON_REG,
				DSC_REG->DISP_DSC_CON, DSC_EN, 1);
	} else {
	#ifdef DSC_RELAY_MODE_EN
		bdg_dsc_init(module, cmdq, dsi);
		BDG_OUTREGBIT(cmdq, struct DISP_DSC_CON_REG,
				DSC_REG->DISP_DSC_CON, DSC_PT_MEM_EN, 1);
		BDG_OUTREGBIT(cmdq, struct DISP_DSC_CON_REG,
				DSC_REG->DISP_DSC_CON, DSC_RELAY, 1);
		BDG_OUTREGBIT(cmdq, struct DISP_DSC_CON_REG,
				DSC_REG->DISP_DSC_CON, DSC_EN, 1);
	#else
		BDG_OUTREGBIT(cmdq, struct DISP_DSC_CON_REG,
				DSC_REG->DISP_DSC_CON, DSC_ALL_BYPASS, 1);
	#endif
	}

	calculate_datarate_cfgs_rx(ap_tx_data_rate);
	startup_seq_common(cmdq);

	if (!dsi->ext->params->is_cphy)
		startup_seq_dphy_specific(ap_tx_data_rate);

	output_debug_signal();

	DDPMSG("%s-\n", __func__);

	return ret;
}

int bdg_common_deinit(enum DISP_BDG_ENUM module, void *cmdq)
{
	int ret = 0;

	DDPMSG("%s----\n", __func__);

	/* close dsi eint */
	atomic_set(&bdg_eint_wakeup, 0);

	/* set spi low speed */
	spislv_switch_speed_hz(SPI_TX_LOW_SPEED_HZ, SPI_RX_LOW_SPEED_HZ);

	set_ana_mipi_dsi_off(cmdq);
	ana_macro_off(cmdq);
	set_mtcmos_off(cmdq);
	set_LDO_off(cmdq);
	need_6382_init = 1;
	clk_buf_disp_ctrl(false);

	return ret;
}

int bdg_common_init_for_rx_pat(enum DISP_BDG_ENUM module,
			struct mtk_dsi *dsi, void *cmdq)
{
	int ret = 0;
	unsigned int vact = 0;
	unsigned int hact = 0;

	DISPSYS_REG = (struct BDG_DISPSYS_CONFIG_REGS *)DISPSYS_BDG_MMSYS_CONFIG_BASE;
	DSI2_REG = (struct BDG_MIPIDSI2_REGS *)DISPSYS_BDG_MIPIDSI2_DEVICE_BASE;
	SYS_REG = (struct BDG_SYSREG_CTRL_REGS *)DISPSYS_BDG_SYSREG_CTRL_BASE;
	RDMA_REG = (struct BDG_RDMA0_REGS *)DISPSYS_BDG_RDMA0_REGS_BASE;
	MUTEX_REG = (struct BDG_MUTEX_REGS *)DISPSYS_BDG_MUTEX_REGS_BASE;
	OCLA_REG = (struct BDG_OCLA_REGS *)DISPSYS_BDG_OCLA_BASE;
	TX_REG[0] = (struct BDG_TX_REGS *)DISPSYS_BDG_TX_DSI0_BASE;
	MIPI_TX_REG = (struct BDG_MIPI_TX_REGS *)DISPSYS_BDG_MIPI_TX_BASE;
	DSC_REG = (struct BDG_DISP_DSC_REGS *)DISPSYS_BDG_DISP_DSC_BASE;
	APMIXEDSYS = (struct BDG_APMIXEDSYS_REGS *)DISPSYS_BDG_APMIXEDSYS_BASE;
	TOPCKGEN = (struct BDG_TOPCKGEN_REGS *)DISPSYS_BDG_TOPCKGEN_BASE;
	GCE_REG = (struct BDG_GCE_REGS *)DISPSYS_BDG_GCE_BASE;
	EFUSE = (struct BDG_EFUSE_REGS *)DISPSYS_BDG_EFUSE_BASE;
	GPIO = (struct BDG_GPIO_REGS *)DISPSYS_BDG_GPIO_BASE;
	TX_CMDQ_REG[0] = (struct DSI_TX_CMDQ_REGS *)(DISPSYS_BDG_TX_DSI0_BASE + 0xd00);

	clk_buf_disp_ctrl(true);//?/
	bdg_tx_pull_6382_reset_pin(dsi);
	set_LDO_on(cmdq);
	set_mtcmos_on(cmdq);
	ana_macro_on(cmdq);
	set_subsys_on(cmdq);

	// Disable reset sequential de-glitch circuit
	BDG_OUTREG32(cmdq, SYS_REG->RST_DG_CTRL, 0);
	// Set GPIO to active IRQ
	BDG_OUTREGBIT(cmdq, struct GPIO_MODE1_REG, GPIO->GPIO_MODE1, GPIO12, 1);

	// for release rx mac reset
	BDG_OUTREG32(cmdq, SYS_REG->DISP_MISC0, 1);
	BDG_OUTREG32(cmdq, SYS_REG->DISP_MISC0, 0);
	// switch DDI(cmd mode) or IPI(vdo mode) path
	BDG_OUTREG32(cmdq, DISPSYS_REG->DDI_POST_CTRL, 0x00001100);
	if ((dsi->mode_flags & MIPI_DSI_MODE_VIDEO) == 0) {
		BDG_OUTREGBIT(cmdq, struct GPIO_MODE1_REG, GPIO->GPIO_MODE1, RSV_24, 0x31);
		BDG_OUTREGBIT(cmdq, struct MIPI_RX_POST_CTRL_REG,
				DISPSYS_REG->MIPI_RX_POST_CTRL, MIPI_RX_MODE_SEL, 0);
	} else
		BDG_OUTREGBIT(cmdq, struct MIPI_RX_POST_CTRL_REG,
				DISPSYS_REG->MIPI_RX_POST_CTRL, MIPI_RX_MODE_SEL, 1);

	// RDMA setting
	vact = mtk_dsi_get_virtual_heigh(dsi, dsi->encoder.crtc);
	hact = mtk_dsi_get_virtual_width(dsi, dsi->encoder.crtc);
	BDG_OUTREG32(cmdq, RDMA_REG->DISP_RDMA_SIZE_CON_0,
			hact);
	BDG_OUTREG32(cmdq, RDMA_REG->DISP_RDMA_SIZE_CON_1,
			vact);

	BDG_OUTREG32(cmdq, RDMA_REG->DISP_RDMA_FIFO_CON, 0xf30 << 16);
	BDG_OUTREG32(cmdq, RDMA_REG->DISP_RDMA_STALL_CG_CON, 0);
	BDG_OUTREGBIT(cmdq, struct DISP_RDMA_SRAM_SEL_REG,
			RDMA_REG->DISP_RDMA_SRAM_SEL, RDMA_SRAM_SEL, 0);
	BDG_OUTREGBIT(cmdq, struct DISP_RDMA_GLOBAL_CON_REG,
			RDMA_REG->DISP_RDMA_GLOBAL_CON, ENGINE_EN, 1);

	// MUTEX setting
	BDG_OUTREG32(cmdq, MUTEX_REG->DISP_MUTEX_INTEN, 0x1 << 16);
	BDG_OUTREG32(cmdq, MUTEX_REG->DISP_MUTEX0_MOD0, 0x1f);
	if ((dsi->mode_flags & MIPI_DSI_MODE_VIDEO) == 0)
		BDG_OUTREG32(cmdq, MUTEX_REG->DISP_MUTEX0_CTL, 0);
	else {
		BDG_OUTREG32(cmdq, MUTEX_REG->DISP_MUTEX0_CTL,
				0x1 << 0 | 0x0 << 3 | 0x1 << 6 | 0x0 << 9);
		BDG_OUTREGBIT(cmdq, struct DISP_MUTEX0_EN_REG,
			MUTEX_REG->DISP_MUTEX0_EN, MUTEX0_EN, 1);
	}

	// DSI-TX setting
	bdg_tx_init(module, dsi, NULL);
	/* panel init*/
	//bdg_lcm_init(pgc->plcm, 1);
	bdg_tx_set_mode(module, cmdq, dsi);
	BDG_OUTREG32(cmdq, TX_REG[0]->DSI_RESYNC_CON, 0x50007);

	// DSC setting
	if (dsc_en) {
		bdg_dsc_init(module, cmdq, dsi);
		BDG_OUTREGBIT(cmdq, struct DISP_DSC_CON_REG,
				DSC_REG->DISP_DSC_CON, DSC_PT_MEM_EN, 1);
		BDG_OUTREGBIT(cmdq, struct DISP_DSC_CON_REG,
				DSC_REG->DISP_DSC_CON, DSC_EN, 1);
	} else {
#ifdef DSC_RELAY_MODE_EN
		bdg_dsc_init(module, cmdq, dsi);
		BDG_OUTREGBIT(cmdq, struct DISP_DSC_CON_REG,
	DSC_REG->DISP_DSC_CON, DSC_PT_MEM_EN, 1);
		BDG_OUTREGBIT(cmdq, struct DISP_DSC_CON_REG,
				DSC_REG->DISP_DSC_CON, DSC_RELAY, 1);
		BDG_OUTREGBIT(cmdq, struct DISP_DSC_CON_REG,
				DSC_REG->DISP_DSC_CON, DSC_EN, 1);
#else
		BDG_OUTREGBIT(cmdq, struct DISP_DSC_CON_REG,
				DSC_REG->DISP_DSC_CON, DSC_ALL_BYPASS, 1);
#endif
	}

	bdg_tx_start(DISP_BDG_DSI0, NULL);

	BDG_OUTREG32(cmdq, DSI2_REG->DSI2_DEVICE_SOFT_RSTN_OS, 0);
	BDG_OUTREG32(cmdq, DSI2_REG->DSI2_DEVICE_IPI_PG_CFG_OS, 4);
	BDG_OUTREG32(cmdq, DSI2_REG->DSI2_DEVICE_IPI_PG_PIXEL_NUM_OS,
			hact);
	if (dsc_en) {
//tx:500M
//		BDG_OUTREG32(cmdq, DSI2_REG->DSI2_DEVICE_IPI_PG_HSA_TIME_OS, 145);
//		BDG_OUTREG32(cmdq, DSI2_REG->DSI2_DEVICE_IPI_PG_HBP_TIME_OS, 145);
//		BDG_OUTREG32(cmdq, DSI2_REG->DSI2_DEVICE_IPI_PG_HLINE_TIME_OS,
//			(1080 + 145 + 145 + 2361));
//tx:760 MHz
		BDG_OUTREG32(cmdq, DSI2_REG->DSI2_DEVICE_IPI_PG_HSA_TIME_OS, 22);
		BDG_OUTREG32(cmdq, DSI2_REG->DSI2_DEVICE_IPI_PG_HBP_TIME_OS, 22);
		BDG_OUTREG32(cmdq, DSI2_REG->DSI2_DEVICE_IPI_PG_HLINE_TIME_OS,
			(hact + 22 + 22 + 1331));
	} else {
//tx:1G
		BDG_OUTREG32(cmdq, DSI2_REG->DSI2_DEVICE_IPI_PG_HSA_TIME_OS, 73);
		BDG_OUTREG32(cmdq, DSI2_REG->DSI2_DEVICE_IPI_PG_HBP_TIME_OS, 73);
		BDG_OUTREG32(cmdq, DSI2_REG->DSI2_DEVICE_IPI_PG_HLINE_TIME_OS,
			(hact + 73 + 73 + 2998));
	}
	BDG_OUTREG32(cmdq, DSI2_REG->DSI2_DEVICE_IPI_PG_VSA_LINES_OS, 20);
	BDG_OUTREG32(cmdq, DSI2_REG->DSI2_DEVICE_IPI_PG_VBP_LINES_OS, 20);
	BDG_OUTREG32(cmdq, DSI2_REG->DSI2_DEVICE_IPI_PG_VFP_LINES_OS, 100);
	BDG_OUTREG32(cmdq, DSI2_REG->DSI2_DEVICE_IPI_PG_VACTIVE_LINES_OS, 2400);
	BDG_OUTREG32(cmdq, DSI2_REG->DSI2_DEVICE_IPI_PG_EN_OS, 1);
	BDG_OUTREG32(cmdq, DSI2_REG->DSI2_DEVICE_SOFT_RSTN_OS, 1);

	return ret;
}

static struct task_struct *bdg_eint_chk_task;
wait_queue_head_t bdg_check_task_wq;
atomic_t bdg_eint_wakeup;
static int mtk_bdg_eint_check_worker_kthread(void *data)
{
	unsigned int irq_ctrl3 = 0;
	unsigned int val;
	struct DSI_TX_INTSTA_REG *intsta;
	unsigned int ret = 0;
	struct sched_param param = {.sched_priority = 87};

	sched_setscheduler(current, SCHED_RR, &param);

	while (1) {
		ret = wait_event_interruptible(bdg_check_task_wq,
						atomic_read(&bdg_eint_wakeup));
		CRTC_MMP_EVENT_START(0, bdg_gce_irq, 0, 0);
		if (ret < 0) {
			DDPINFO("[ESD]check thread waked up accidently\n");
			continue;
		}
		irq_ctrl3 = mtk_spi_read((unsigned long)(&SYS_REG->SYSREG_IRQ_CTRL3));
		DDPDBG("%s, mt6382 irq_ctrl3: (0x%x)\n", __func__, irq_ctrl3);

		if (irq_ctrl3 & BIT(4)) {
			val = mtk_spi_read((unsigned long)&TX_REG[0]->DSI_TX_INTSTA);
			intsta = (struct DSI_TX_INTSTA_REG *)&val;

			if (intsta->FRAME_DONE_INT_FLAG)
				CRTC_MMP_MARK(0, bdg_gce_irq, 1, 1);

			if (intsta->BUFFER_UNDERRUN_INT_FLAG) {
				CRTC_MMP_MARK(0, bdg_gce_irq, 2, 2);
				bdg_dsi_dump_reg(DISP_BDG_DSI0);
				DDPAEE("disp bdg 6382 underrun\n");
			}

			BDG_OUTREG32(NULL, TX_REG[0]->DSI_TX_INTSTA, ~val);
		}

		atomic_set(&bdg_eint_wakeup, 0);

		if (kthread_should_stop())
			break;
		CRTC_MMP_EVENT_END(0, bdg_gce_irq, 0, 0);
	}
	return ret;
}

irqreturn_t bdg_eint_thread_handler(int irq, void *data)
{
	wake_up_interruptible(&bdg_check_task_wq);
	atomic_set(&bdg_eint_wakeup, 1);

	return IRQ_HANDLED;
}

void bdg_request_eint_irq(void)
{
	struct device_node *node;

	if (irq_already_requested) {
		DDPMSG("%s irq_already_requested\n", __func__);
		return;
	}

	// get compatible node
	node = of_find_compatible_node(NULL, NULL, "mediatek,mt6382_eint");
	if (!node) {
		DDPMSG("[error]%s, mt6382 can't find mt6382_eint compatible node\n", __func__);
		return;
	}

	// get irq number
	bdg_eint_irq = irq_of_parse_and_map(node, 0);
	DDPMSG("%s, mt6382 EINT irq number: (%d)\n", __func__, bdg_eint_irq);

	// register irq thread handler
	if (request_threaded_irq(bdg_eint_irq, NULL/*dbg_eint_irq_handler*/,
				bdg_eint_thread_handler, IRQF_TRIGGER_RISING | IRQF_ONESHOT,
				"MT6382_EINT", NULL)) {
		DDPMSG("[error]%s, mt6382 request EINT irq failed!\n", __func__);
		return;
	}

	irq_already_requested = true;

	bdg_eint_chk_task = kthread_create(
		mtk_bdg_eint_check_worker_kthread, NULL, "bdg_6382");
	init_waitqueue_head(&bdg_check_task_wq);
	wake_up_process(bdg_eint_chk_task);
	atomic_set(&bdg_eint_wakeup, 1);

}

void bdg_dsi_mipi_clk_change(enum DISP_BDG_ENUM module, void *handle, int clk)
{
	unsigned int pcw_ratio = 0;
	unsigned int pcw = 0;
	unsigned int pcw_floor = 0;
	unsigned int posdiv    = 0;
	unsigned int prediv    = 0;
	unsigned int status = 0;

	if (clk != 0) {
		unsigned int tmp = 0;

		if (clk > 2500) {
			DDPMSG("[error]mipitx Data Rate exceed limit(%d)\n",
			clk);
			return;
		} else if (clk >= 2000) { /* 2G ~ 2.5G */
			pcw_ratio = 1;
			posdiv    = 0;
			prediv    = 0;
		} else if (clk >= 1000) { /* 1G ~ 2G */
			pcw_ratio = 2;
			posdiv    = 1;
			prediv    = 0;
		} else if (clk >= 500) { /* 500M ~ 1G */
			pcw_ratio = 4;
			posdiv    = 2;
			prediv    = 0;
		} else if (clk > 250) { /* 250M ~ 500M */
			pcw_ratio = 8;
			posdiv    = 3;
			prediv    = 0;
		} else if (clk >= 125) { /* 125M ~ 250M */
			pcw_ratio = 16;
			posdiv    = 4;
			prediv    = 0;
		} else {
			DDPMSG("[error]dataRate is too low(%d)\n", clk);
			return;
		}

		pcw = clk * pcw_ratio / 26;
		pcw_floor = clk * pcw_ratio % 26;
		tmp = ((pcw & 0xFF) << 24) |
		(((256 * pcw_floor / 26) & 0xFF) << 16) |
		(((256 * (256 * pcw_floor % 26) / 26) & 0xFF) << 8) |
		((256 * (256 * (256 * pcw_floor % 26) % 26) / 26)
		& 0xFF);

		BDG_OUTREG32(handle, MIPI_TX_REG->MIPI_TX_PLL_CON0, tmp);
		BDG_OUTREGBIT(handle, struct MIPI_TX_PLL_CON1_REG,
				MIPI_TX_REG->MIPI_TX_PLL_CON1,
				RG_DSI_PLL_POSDIV, posdiv);

		status = mtk_spi_read((unsigned long)(&MIPI_TX_REG->MIPI_TX_PLL_CON1));
		if ((status & 0x1) == 0x1)
			BDG_OUTREGBIT(handle, struct MIPI_TX_PLL_CON1_REG,
				MIPI_TX_REG->MIPI_TX_PLL_CON1,
				RG_DSI_PLL_SDM_PCW_CHG, 0);
		else
			BDG_OUTREGBIT(handle, struct MIPI_TX_PLL_CON1_REG,
				MIPI_TX_REG->MIPI_TX_PLL_CON1,
				RG_DSI_PLL_SDM_PCW_CHG, 1);
	}
}

void bdg_dsi_porch_setting(enum DISP_BDG_ENUM module, void *handle,
	 struct mtk_dsi *dsi)
{
	u32 dsi_buf_bpp, data_init_byte;
	u32 t_hfp, t_hbp, t_hsa;
	u32 hsa, hbp, hfp;
	struct dynamic_mipi_params *dyn = &dsi->ext->params->dyn;
	struct videomode *vm = &dsi->vm;

	if (!dyn->hfp && !dyn->hbp && !dyn->hsa) {
		DDPMSG("[error]%s, the dyn h porch is null\n", __func__);
		return;
	}

	t_hfp = (dsi->bdg_mipi_hopping_sta) ?
			((dyn && !!dyn->hfp) ?
			 dyn->hfp : vm->hfront_porch) :
			vm->hfront_porch;

	t_hbp = (dsi->bdg_mipi_hopping_sta) ?
			((dyn && !!dyn->hbp) ?
			 dyn->hbp : vm->hback_porch) :
			vm->hback_porch;

	t_hsa = (dsi->bdg_mipi_hopping_sta) ?
			((dyn && !!dyn->hsa) ?
			 dyn->hsa : vm->hsync_len) :
			vm->hsync_len;

	if (dsi->format == MIPI_DSI_FMT_RGB565)
		dsi_buf_bpp = 16;
	else
		dsi_buf_bpp = 24;

	if (dsi->ext->params->is_cphy) {
		DDPMSG("C-PHY mode, need check!!!\n");
	} else {
		data_init_byte = bg_tx_data_phy_cycle * dsi->lanes;

		if (dsi->mode_flags & MIPI_DSI_MODE_VIDEO) {
			if (dsi->mode_flags & MIPI_DSI_MODE_VIDEO_SYNC_PULSE) {
				hsa = (((t_hsa * dsi_buf_bpp) / 8) - 10);
				hbp = (((t_hbp * dsi_buf_bpp) / 8) - 10);
				hfp = (((t_hfp * dsi_buf_bpp) / 8) - 12);
			} else if (dsi->mode_flags & MIPI_DSI_MODE_VIDEO_BURST) {
				hsa = 0;	/* don't care */
				hbp = (((t_hbp + t_hsa) * dsi_buf_bpp) / 8) - 10;
				hfp = (((t_hfp * dsi_buf_bpp) / 8) - 12 - 6);
			} else {
				hsa = 0;	/* don't care */
				hbp = (((t_hbp + t_hsa) * dsi_buf_bpp) / 8) - 10;
				hfp = (((t_hfp * dsi_buf_bpp) / 8) - 12);
			}
		}
	}

	if (hsa < 0) {
		DDPMSG("error!hsa = %d < 0!\n", hsa);
		hsa = 0;
	}

	if (hfp > data_init_byte)
		hfp -= data_init_byte;
	else {
		hfp = 4;
		DDPMSG("hfp is too short!\n");
	}

	if (dyn->hfp)
		BDG_OUTREG32(handle, TX_REG[0]->DSI_TX_HSA_WC, hfp);
	else if (dyn->hsa)
		BDG_OUTREG32(handle, TX_REG[0]->DSI_TX_HSA_WC, hsa);
	else if (dyn->hbp)
		BDG_OUTREG32(handle, TX_REG[0]->DSI_TX_HBP_WC, hbp);

}

void bdg_mipi_clk_change(enum DISP_BDG_ENUM module,
			struct mtk_dsi *dsi, void *cmdq)
{
	struct mtk_panel_ext *ext = dsi->ext;
	unsigned int data_rate;

	if (!(ext && ext->params &&
			ext->params->dyn.switch_en == 1))
		return;

	if (dsi->bdg_mipi_hopping_sta)
		data_rate = !!ext->params->dyn.data_rate ?
				ext->params->dyn.data_rate :
				ext->params->dyn.pll_clk * 2;

	/* change mipi clk & hbp porch params*/
	bdg_dsi_mipi_clk_change(DISP_BDG_DSI0, cmdq, data_rate);
	bdg_dsi_porch_setting(DISP_BDG_DSI0, cmdq, dsi);
}

void bdg_clk_buf_nfc(bool onoff)
{
//	DISPFUNCSTART();

	if (onoff) {
		mtk_spi_write(0x000000a0, 0x00000022);
//		DSI_OUTREGBIT(cmdq, struct CKBUF_CTRL_REG,
//				SYS_REG->CKBUF_CTRL, NFC_CK_OUT_EN, 1);
	} else {
		mtk_spi_write(0x000000a0, 0x00000002);
//		DSI_OUTREGBIT(cmdq, struct CKBUF_CTRL_REG,
//				SYS_REG->CKBUF_CTRL, NFC_CK_OUT_EN, 0);
	}
//	DISPFUNCEND();
}

/***** NFC SRCLKENAI0 Interrupt Handler +++ *****/
void nfc_work_func(void)
{
	int nfc_srclk;

	nfc_srclk = gpio_get_value(mt6382_nfc_srclk);
	//DDPMSG("%s, NFC SRCLK GPIO Value = %d\n", __func__, nfc_srclk);

	//suspend need to disable MT6382 first and enable SRCLK first
	if (nfc_srclk != mt6382_nfc_gpio_value) { //the state of gpio has been updated
		mt6382_nfc_gpio_value = nfc_srclk;

		if (nfc_srclk == 1) {
			//switch the mt6382 clock
			//DDPMSG("%s, NFC SRCLK switch the display clock = %d\n",
			//	__func__, nfc_srclk);
			nfc_clk_already_enabled = true;
			bdg_clk_buf_nfc(nfc_srclk);
		} else {
			//switch the mt6382 clock
			//DDPMSG("%s, NFC SRCLK switch the display clock = %d\n",
			//	__func__, nfc_srclk);
			nfc_clk_already_enabled = false;
			bdg_clk_buf_nfc(nfc_srclk);
		}
	}
}

irqreturn_t nfc_eint_thread_handler(int irq, void *data)
{
	nfc_work_func();

	return IRQ_HANDLED;
}

void nfc_request_eint_irq(void)
{
	struct device_node *node;

	if (nfc_irq_already_requested) {
		enable_irq(nfc_eint_irq);
		return;
	}

	// get compatible node
	node = of_find_compatible_node(NULL, NULL, "mediatek, mt6382_nfc-eint");
	if (!node) {
		DDPMSG("%s, mt6382 can't find mt6382_nfc_eint compatible node\n", __func__);
		return;
	}

	//get gpio
	mt6382_nfc_srclk = of_get_named_gpio(node, "mt6382_nfc_srclk", 0);
	if (mt6382_nfc_srclk < 0)
		DDPMSG("%s: get NFC SRCLK GPIO failed (%d)", __func__, mt6382_nfc_srclk);
	else
		DDPMSG("%s: get NFC SRCLK GPIO Success (%d)", __func__, mt6382_nfc_srclk);

	// get irq number
	nfc_eint_irq = irq_of_parse_and_map(node, 0);
	DDPMSG("%s, mt6382 NFC EINT irq number: (%d)\n", __func__, nfc_eint_irq);

	// register irq thread handler
	if (request_threaded_irq(nfc_eint_irq, NULL/*dbg_eint_irq_handler*/,
				nfc_eint_thread_handler,
				IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING | IRQF_ONESHOT,
				"MT6382_NFC_EINT", NULL)) {
		DDPMSG("%s, mt6382 request NFC EINT irq failed!\n", __func__);
		return;
	}

	nfc_irq_already_requested = true;
	mt6382_nfc_gpio_value = 0;

	//get SRCLK status
	nfc_work_func();

	// enable irq
	enable_irq(nfc_eint_irq);
	// enable irq wake
	irq_set_irq_wake(nfc_eint_irq, 1);
}
/***** NFC SRCLKENAI0 Interrupt Handler --- *****/
