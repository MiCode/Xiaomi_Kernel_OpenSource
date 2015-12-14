/* Copyright (c) 2015, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

/* Qualcomm Technologies Inc EMAC SGMII Controller driver.
 */

#include "emac.h"
#include "emac_hw.h"

/* EMAC_QSERDES register offsets */
#define EMAC_QSERDES_COM_SYS_CLK_CTRL            0x000000
#define EMAC_QSERDES_COM_PLL_CNTRL               0x000014
#define EMAC_QSERDES_COM_PLL_IP_SETI             0x000018
#define EMAC_QSERDES_COM_PLL_CP_SETI             0x000024
#define EMAC_QSERDES_COM_PLL_IP_SETP             0x000028
#define EMAC_QSERDES_COM_PLL_CP_SETP             0x00002c
#define EMAC_QSERDES_COM_SYSCLK_EN_SEL           0x000038
#define EMAC_QSERDES_COM_RESETSM_CNTRL           0x000040
#define EMAC_QSERDES_COM_PLLLOCK_CMP1            0x000044
#define EMAC_QSERDES_COM_PLLLOCK_CMP2            0x000048
#define EMAC_QSERDES_COM_PLLLOCK_CMP3            0x00004c
#define EMAC_QSERDES_COM_PLLLOCK_CMP_EN          0x000050
#define EMAC_QSERDES_COM_DEC_START1              0x000064
#define EMAC_QSERDES_COM_DIV_FRAC_START1         0x000098
#define EMAC_QSERDES_COM_DIV_FRAC_START2         0x00009c
#define EMAC_QSERDES_COM_DIV_FRAC_START3         0x0000a0
#define EMAC_QSERDES_COM_DEC_START2              0x0000a4
#define EMAC_QSERDES_COM_PLL_CRCTRL              0x0000ac
#define EMAC_QSERDES_COM_RESET_SM                0x0000bc
#define EMAC_QSERDES_TX_BIST_MODE_LANENO         0x000100
#define EMAC_QSERDES_TX_TX_EMP_POST1_LVL         0x000108
#define EMAC_QSERDES_TX_TX_DRV_LVL               0x00010c
#define EMAC_QSERDES_TX_LANE_MODE                0x000150
#define EMAC_QSERDES_TX_TRAN_DRVR_EMP_EN         0x000170
#define EMAC_QSERDES_RX_CDR_CONTROL              0x000200
#define EMAC_QSERDES_RX_CDR_CONTROL2             0x000210
#define EMAC_QSERDES_RX_RX_EQ_GAIN12             0x000230

/* EMAC_SGMII register offsets */
#define EMAC_SGMII_PHY_SERDES_START              0x000300
#define EMAC_SGMII_PHY_CMN_PWR_CTRL              0x000304
#define EMAC_SGMII_PHY_RX_PWR_CTRL               0x000308
#define EMAC_SGMII_PHY_TX_PWR_CTRL               0x00030C
#define EMAC_SGMII_PHY_LANE_CTRL1                0x000318
#define EMAC_SGMII_PHY_AUTONEG_CFG2              0x000348
#define EMAC_SGMII_PHY_CDR_CTRL0                 0x000358
#define EMAC_SGMII_PHY_SPEED_CFG1                0x000374
#define EMAC_SGMII_PHY_POW_DWN_CTRL0             0x000380
#define EMAC_SGMII_PHY_RESET_CTRL                0x0003a8
#define EMAC_SGMII_PHY_IRQ_CMD                   0x0003ac
#define EMAC_SGMII_PHY_INTERRUPT_CLEAR           0x0003b0
#define EMAC_SGMII_PHY_INTERRUPT_MASK            0x0003b4
#define EMAC_SGMII_PHY_INTERRUPT_STATUS          0x0003b8
#define EMAC_SGMII_PHY_RX_CHK_STATUS             0x0003d4
#define EMAC_SGMII_PHY_AUTONEG0_STATUS           0x0003e0
#define EMAC_SGMII_PHY_AUTONEG1_STATUS           0x0003e4

#define SERDES_START_WAIT_TIMES         100

#define SGMII_CDR_MAX_CNT               0x0f

#define QSERDES_PLL_IPSETI              0x01
#define QSERDES_PLL_CP_SETI             0x3b
#define QSERDES_PLL_IP_SETP             0x0a
#define QSERDES_PLL_CP_SETP             0x09
#define QSERDES_PLL_CRCTRL              0xfb
#define QSERDES_PLL_DEC                    2
#define QSERDES_PLL_DIV_FRAC_START1     0x55
#define QSERDES_PLL_DIV_FRAC_START2     0x2a
#define QSERDES_PLL_DIV_FRAC_START3     0x03
#define QSERDES_PLL_LOCK_CMP1           0x2b
#define QSERDES_PLL_LOCK_CMP2           0x68
#define QSERDES_PLL_LOCK_CMP3           0x00

#define QSERDES_RX_CDR_CTRL1_THRESH     0x03
#define QSERDES_RX_CDR_CTRL1_GAIN       0x02
#define QSERDES_RX_CDR_CTRL2_THRESH     0x03
#define QSERDES_RX_CDR_CTRL2_GAIN       0x04
#define QSERDES_RX_EQ_GAIN2              0xf
#define QSERDES_RX_EQ_GAIN1              0xf

#define QSERDES_TX_BIST_MODE_LANENO     0x00
#define QSERDES_TX_DRV_LVL              0x0f
#define QSERDES_TX_EMP_POST1_LVL           1
#define QSERDES_TX_LANE_MODE            0x08

#define SGMII_PHY_IRQ_CLR_WAIT_TIME     10

#define SGMII_PHY_INTERRUPT_ERR (\
	DECODE_CODE_ERR         |\
	DECODE_DISP_ERR)

#define SGMII_ISR_AN_MASK       (\
	AN_REQUEST              |\
	AN_START                |\
	AN_END                  |\
	AN_ILLEGAL_TERM         |\
	PLL_UNLOCK              |\
	SYNC_FAIL)

#define SGMII_ISR_MASK          (\
	SGMII_PHY_INTERRUPT_ERR |\
	SGMII_ISR_AN_MASK)

/* SGMII TX_CONFIG */
#define TXCFG_LINK                      0x8000
#define TXCFG_MODE_BMSK                 0x1c00
#define TXCFG_1000_FULL                 0x1800
#define TXCFG_100_FULL                  0x1400
#define TXCFG_100_HALF                  0x0400
#define TXCFG_10_FULL                   0x1000
#define TXCFG_10_HALF                   0x0000

struct emac_sgmii_v1 {
	void __iomem	*base;
	int		irq;
};

static int emac_sgmii_v1_config(struct platform_device *pdev,
				struct emac_adapter *adpt)
{
	struct emac_sgmii_v1 *sgmii;
	struct resource *res;
	int ret;

	sgmii = devm_kzalloc(&pdev->dev, sizeof(*sgmii), GFP_KERNEL);
	if (!sgmii)
		return -ENOMEM;

	ret = platform_get_irq_byname(pdev, "emac_sgmii_irq");
	if (ret < 0)
		return ret;

	sgmii->irq = ret;

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "emac_sgmii");
	if (!res) {
		emac_err(adpt,
			 "error platform_get_resource_byname(emac_sgmii)\n");
		return -ENOENT;
	}

	sgmii->base = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(sgmii->base)) {
		emac_err(adpt,
			 "error:%ld devm_ioremap_resource(start:0x%lx size:0x%lx)\n",
			 PTR_ERR(sgmii->base), (ulong) res->start,
			 (ulong) resource_size(res));
		return -ENOMEM;
	}

	adpt->phy.private = sgmii;

	return 0;
}

/* LINK */
static int emac_sgmii_v1_init_link(struct emac_adapter *adpt, u32 speed,
				   bool autoneg, bool fc)
{
	struct emac_sgmii_v1 *sgmii = adpt->phy.private;
	u32 val;
	u32 speed_cfg = 0;

	val = readl_relaxed(sgmii->base + EMAC_SGMII_PHY_AUTONEG_CFG2);

	if (autoneg) {
		val &= ~(FORCE_AN_RX_CFG | FORCE_AN_TX_CFG);
		val |= AN_ENABLE;
		writel_relaxed(val, sgmii->base + EMAC_SGMII_PHY_AUTONEG_CFG2);
	} else {
		switch (speed) {
		case EMAC_LINK_SPEED_10_HALF:
			speed_cfg = SPDMODE_10;
			break;
		case EMAC_LINK_SPEED_10_FULL:
			speed_cfg = SPDMODE_10 | DUPLEX_MODE;
			break;
		case EMAC_LINK_SPEED_100_HALF:
			speed_cfg = SPDMODE_100;
			break;
		case EMAC_LINK_SPEED_100_FULL:
			speed_cfg = SPDMODE_100 | DUPLEX_MODE;
			break;
		case EMAC_LINK_SPEED_1GB_FULL:
			speed_cfg = SPDMODE_1000 | DUPLEX_MODE;
			break;
		default:
			return -EINVAL;
		}
		val &= ~AN_ENABLE;
		writel_relaxed(speed_cfg,
			       sgmii->base + EMAC_SGMII_PHY_SPEED_CFG1);
		writel_relaxed(val, sgmii->base + EMAC_SGMII_PHY_AUTONEG_CFG2);
	}
	/* Ensure Auto-Neg setting are written to HW before leaving */
	wmb();

	return 0;
}

static int emac_hw_clear_sgmii_intr_status(struct emac_adapter *adpt,
					   u32 irq_bits)
{
	struct emac_sgmii_v1 *sgmii = adpt->phy.private;
	u32 status;
	int i;

	writel_relaxed(irq_bits, sgmii->base + EMAC_SGMII_PHY_INTERRUPT_CLEAR);
	writel_relaxed(IRQ_GLOBAL_CLEAR, sgmii->base + EMAC_SGMII_PHY_IRQ_CMD);
	/* Ensure interrupt clear command is written to HW */
	wmb();

	/* After set the IRQ_GLOBAL_CLEAR bit, the status clearing must
	 * be confirmed before clearing the bits in other registers.
	 * It takes a few cycles for hw to clear the interrupt status.
	 */
	for (i = 0; i < SGMII_PHY_IRQ_CLR_WAIT_TIME; i++) {
		udelay(1);
		status = readl_relaxed(sgmii->base +
				       EMAC_SGMII_PHY_INTERRUPT_STATUS);
		if (!(status & irq_bits))
			break;
	}
	if (status & irq_bits) {
		emac_err(adpt,
			 "failed to clear SGMII irq: status 0x%x bits 0x%x\n",
			 status, irq_bits);
		return -EIO;
	}

	/* Finalize clearing procedure */
	writel_relaxed(0, sgmii->base + EMAC_SGMII_PHY_IRQ_CMD);
	writel_relaxed(0, sgmii->base + EMAC_SGMII_PHY_INTERRUPT_CLEAR);
	/* Ensure that clearing procedure finalization is written to HW */
	wmb();

	return 0;
}

struct emac_reg_write {
	ulong		offset;
	u32		val;
};

#define END_MARKER 0xffffffff

static const struct emac_reg_write physical_coding_sublayer_programming[] = {
{EMAC_SGMII_PHY_CDR_CTRL0,	SGMII_CDR_MAX_CNT},
{EMAC_SGMII_PHY_POW_DWN_CTRL0,	PWRDN_B},
{EMAC_SGMII_PHY_CMN_PWR_CTRL,	BIAS_EN | SYSCLK_EN | CLKBUF_L_EN |
				PLL_TXCLK_EN | PLL_RXCLK_EN},
{EMAC_SGMII_PHY_TX_PWR_CTRL,	L0_TX_EN | L0_CLKBUF_EN | L0_TRAN_BIAS_EN},
{EMAC_SGMII_PHY_RX_PWR_CTRL,	L0_RX_SIGDET_EN | (1 << L0_RX_TERM_MODE_SHFT) |
				L0_RX_I_EN},
{EMAC_SGMII_PHY_CMN_PWR_CTRL,	BIAS_EN | PLL_EN | SYSCLK_EN | CLKBUF_L_EN |
				PLL_TXCLK_EN | PLL_RXCLK_EN},
{EMAC_SGMII_PHY_LANE_CTRL1,	L0_RX_EQ_EN | L0_RESET_TSYNC_EN |
				L0_DRV_LVL_BMSK},
{END_MARKER,			END_MARKER},
};

static const struct emac_reg_write sysclk_refclk_setting[] = {
{EMAC_QSERDES_COM_SYSCLK_EN_SEL,	SYSCLK_SEL_CMOS},
{EMAC_QSERDES_COM_SYS_CLK_CTRL,		SYSCLK_CM | SYSCLK_AC_COUPLE},
{END_MARKER,				END_MARKER},
};

static const struct emac_reg_write pll_setting[] = {
{EMAC_QSERDES_COM_PLL_IP_SETI,		QSERDES_PLL_IPSETI},
{EMAC_QSERDES_COM_PLL_CP_SETI,		QSERDES_PLL_CP_SETI},
{EMAC_QSERDES_COM_PLL_IP_SETP,		QSERDES_PLL_IP_SETP},
{EMAC_QSERDES_COM_PLL_CP_SETP,		QSERDES_PLL_CP_SETP},
{EMAC_QSERDES_COM_PLL_CRCTRL,		QSERDES_PLL_CRCTRL},
{EMAC_QSERDES_COM_PLL_CNTRL,		OCP_EN | PLL_DIV_FFEN | PLL_DIV_ORD},
{EMAC_QSERDES_COM_DEC_START1,		DEC_START1_MUX | QSERDES_PLL_DEC},
{EMAC_QSERDES_COM_DEC_START2,		DEC_START2_MUX | DEC_START2},
{EMAC_QSERDES_COM_DIV_FRAC_START1,	DIV_FRAC_START1_MUX |
					QSERDES_PLL_DIV_FRAC_START1},
{EMAC_QSERDES_COM_DIV_FRAC_START2,	DIV_FRAC_START2_MUX |
					QSERDES_PLL_DIV_FRAC_START2},
{EMAC_QSERDES_COM_DIV_FRAC_START3,	DIV_FRAC_START3_MUX |
					QSERDES_PLL_DIV_FRAC_START3},
{EMAC_QSERDES_COM_PLLLOCK_CMP1,		QSERDES_PLL_LOCK_CMP1},
{EMAC_QSERDES_COM_PLLLOCK_CMP2,		QSERDES_PLL_LOCK_CMP2},
{EMAC_QSERDES_COM_PLLLOCK_CMP3,		QSERDES_PLL_LOCK_CMP3},
{EMAC_QSERDES_COM_PLLLOCK_CMP_EN,	PLLLOCK_CMP_EN},
{EMAC_QSERDES_COM_RESETSM_CNTRL,	FRQ_TUNE_MODE},
{END_MARKER,				END_MARKER},
};

static const struct emac_reg_write cdr_setting[] = {
{EMAC_QSERDES_RX_CDR_CONTROL,	SECONDORDERENABLE |
		(QSERDES_RX_CDR_CTRL1_THRESH << FIRSTORDER_THRESH_SHFT) |
		(QSERDES_RX_CDR_CTRL1_GAIN << SECONDORDERGAIN_SHFT)},
{EMAC_QSERDES_RX_CDR_CONTROL2,	SECONDORDERENABLE |
		(QSERDES_RX_CDR_CTRL2_THRESH << FIRSTORDER_THRESH_SHFT) |
		(QSERDES_RX_CDR_CTRL2_GAIN << SECONDORDERGAIN_SHFT)},
{END_MARKER,				END_MARKER},
};

static const struct emac_reg_write tx_rx_setting[] = {
{EMAC_QSERDES_TX_BIST_MODE_LANENO,	QSERDES_TX_BIST_MODE_LANENO},
{EMAC_QSERDES_TX_TX_DRV_LVL,		TX_DRV_LVL_MUX |
			(QSERDES_TX_DRV_LVL << TX_DRV_LVL_SHFT)},
{EMAC_QSERDES_TX_TRAN_DRVR_EMP_EN,	EMP_EN_MUX | EMP_EN},
{EMAC_QSERDES_TX_TX_EMP_POST1_LVL,	TX_EMP_POST1_LVL_MUX |
			(QSERDES_TX_EMP_POST1_LVL << TX_EMP_POST1_LVL_SHFT)},
{EMAC_QSERDES_RX_RX_EQ_GAIN12,
				(QSERDES_RX_EQ_GAIN2 << RX_EQ_GAIN2_SHFT) |
				(QSERDES_RX_EQ_GAIN1 << RX_EQ_GAIN1_SHFT)},
{EMAC_QSERDES_TX_LANE_MODE,		QSERDES_TX_LANE_MODE},
{END_MARKER,				END_MARKER},
};

static void emac_reg_write_all(struct emac_sgmii_v1 *sgmii,
			       const struct emac_reg_write *itr)
{
	for (; itr->offset != END_MARKER; ++itr)
		writel_relaxed(itr->val, sgmii->base + itr->offset);
}

static int emac_sgmii_v1_init(struct emac_adapter *adpt)
{
	int i;
	struct emac_phy *phy = &adpt->phy;
	struct emac_sgmii_v1 *sgmii = phy->private;

	emac_sgmii_v1_init_link(adpt, phy->autoneg_advertised,
				phy->autoneg, !phy->disable_fc_autoneg);

	emac_reg_write_all(sgmii, physical_coding_sublayer_programming);

	/* Ensure Rx/Tx lanes power configuration is written to hw before
	 * configuring the SerDes engine's clocks
	 */
	wmb();

	emac_reg_write_all(sgmii, sysclk_refclk_setting);
	emac_reg_write_all(sgmii, pll_setting);
	emac_reg_write_all(sgmii, cdr_setting);
	emac_reg_write_all(sgmii, tx_rx_setting);

	/* Ensure SerDes engine configuration is written to hw before powering
	 * it up
	 */
	wmb();

	writel_relaxed(SERDES_START, sgmii->base + EMAC_SGMII_PHY_SERDES_START);

	/* Ensure Rx/Tx SerDes engine power-up command is written to HW */
	wmb();

	for (i = 0; i < SERDES_START_WAIT_TIMES; i++) {
		if (readl_relaxed(sgmii->base + EMAC_QSERDES_COM_RESET_SM) &
		    QSERDES_READY)
			break;
		usleep_range(100, 200);
	}

	if (i == SERDES_START_WAIT_TIMES) {
		emac_err(adpt, "serdes failed to start\n");
		return -EIO;
	}
	/* Mask out all the SGMII Interrupt */
	writel_relaxed(0, sgmii->base + EMAC_SGMII_PHY_INTERRUPT_MASK);
	/* Ensure SGMII interrupts are masked out before clearing them */
	wmb();

	emac_hw_clear_sgmii_intr_status(adpt, SGMII_PHY_INTERRUPT_ERR);

	return 0;
}

static int emac_sgmii_v1_reset_impl(struct emac_adapter *adpt)
{
	struct emac_sgmii_v1 *sgmii = adpt->phy.private;
	u32 val;

	val = readl_relaxed(sgmii->base + EMAC_EMAC_WRAPPER_CSR2);
	writel_relaxed(((val & ~PHY_RESET) | PHY_RESET),
		       sgmii->base + EMAC_EMAC_WRAPPER_CSR2);
	/* Ensure phy-reset command is written to HW before the release cmd */
	wmb();
	msleep(50);
	val = readl_relaxed(sgmii->base + EMAC_EMAC_WRAPPER_CSR2);
	writel_relaxed((val & ~PHY_RESET),
		       sgmii->base + EMAC_EMAC_WRAPPER_CSR2);
	/* Ensure phy-reset release command is written to HW before initializing
	 * SGMII
	 */
	wmb();
	msleep(50);
	return emac_sgmii_v1_init(adpt);
}

static void emac_sgmii_v1_reset(struct emac_adapter *adpt)
{
	emac_clk_set_rate(adpt, EMAC_CLK_125M, EMC_CLK_RATE_19_2MHZ);
	emac_sgmii_v1_reset_impl(adpt);
	emac_clk_set_rate(adpt, EMAC_CLK_125M, EMC_CLK_RATE_125MHZ);
}

static int emac_sgmii_v1_init_ephy_nop(struct emac_adapter *adpt)
{
	return 0;
}

int emac_sgmii_v1_link_setup_no_ephy(struct emac_adapter *adpt, u32 speed,
				     bool autoneg)
{
	struct emac_phy *phy = &adpt->phy;

	phy->autoneg		= autoneg;
	phy->autoneg_advertised	= speed;
	/* The AN_ENABLE and SPEED_CFG can't change on fly. The SGMII_PHY has
	 * to be re-initialized.
	 */
	return emac_sgmii_v1_reset_impl(adpt);
}

static int emac_sgmii_v1_autoneg_check(struct emac_adapter *adpt, u32 *speed,
				       bool *link_up)
{
	struct emac_sgmii_v1 *sgmii = adpt->phy.private;
	u32 autoneg0, autoneg1, status;

	autoneg0 = readl_relaxed(sgmii->base + EMAC_SGMII_PHY_AUTONEG0_STATUS);
	autoneg1 = readl_relaxed(sgmii->base + EMAC_SGMII_PHY_AUTONEG1_STATUS);
	status   = ((autoneg1 & 0xff) << 8) | (autoneg0 & 0xff);

	if (!(status & TXCFG_LINK)) {
		*link_up = false;
		*speed = EMAC_LINK_SPEED_UNKNOWN;
		return 0;
	}

	*link_up = true;

	switch (status & TXCFG_MODE_BMSK) {
	case TXCFG_1000_FULL:
		*speed = EMAC_LINK_SPEED_1GB_FULL;
		break;
	case TXCFG_100_FULL:
		*speed = EMAC_LINK_SPEED_100_FULL;
		break;
	case TXCFG_100_HALF:
		*speed = EMAC_LINK_SPEED_100_HALF;
		break;
	case TXCFG_10_FULL:
		*speed = EMAC_LINK_SPEED_10_FULL;
		break;
	case TXCFG_10_HALF:
		*speed = EMAC_LINK_SPEED_10_HALF;
		break;
	default:
		*speed = EMAC_LINK_SPEED_UNKNOWN;
		break;
	}
	return 0;
}

static  int emac_sgmii_v1_link_check_no_ephy(struct emac_adapter *adpt,
					     u32 *speed, bool *link_up)
{
	struct emac_sgmii_v1 *sgmii = adpt->phy.private;
	u32 val;

	val = readl_relaxed(sgmii->base + EMAC_SGMII_PHY_AUTONEG_CFG2);
	if (val & AN_ENABLE)
		return emac_sgmii_v1_autoneg_check(adpt, speed, link_up);

	val = readl_relaxed(sgmii->base + EMAC_SGMII_PHY_SPEED_CFG1);
	val &= DUPLEX_MODE | SPDMODE_BMSK;
	switch (val) {
	case DUPLEX_MODE | SPDMODE_1000:
		*speed = EMAC_LINK_SPEED_1GB_FULL;
		break;
	case DUPLEX_MODE | SPDMODE_100:
		*speed = EMAC_LINK_SPEED_100_FULL;
		break;
	case SPDMODE_100:
		*speed = EMAC_LINK_SPEED_100_HALF;
		break;
	case DUPLEX_MODE | SPDMODE_10:
		*speed = EMAC_LINK_SPEED_10_FULL;
		break;
	case SPDMODE_10:
		*speed = EMAC_LINK_SPEED_10_HALF;
		break;
	default:
		*speed = EMAC_LINK_SPEED_UNKNOWN;
		break;
	}
	*link_up = true;
	return 0;
}

static irqreturn_t emac_sgmii_v1_isr(int _irq, void *data)
{
	struct emac_adapter *adpt = data;
	struct emac_sgmii_v1 *sgmii = adpt->phy.private;
	u32 status;

	emac_dbg(adpt, intr, "receive sgmii interrupt\n");

	do {
		status = readl_relaxed(sgmii->base +
				       EMAC_SGMII_PHY_INTERRUPT_STATUS) &
				       SGMII_ISR_MASK;
		if (!status)
			break;

		if (status & SGMII_PHY_INTERRUPT_ERR) {
			SET_FLAG(adpt, ADPT_TASK_CHK_SGMII_REQ);
			if (!TEST_FLAG(adpt, ADPT_STATE_DOWN))
				emac_task_schedule(adpt);
		}

		if (status & SGMII_ISR_AN_MASK)
			emac_check_lsc(adpt);

		if (emac_hw_clear_sgmii_intr_status(adpt, status) != 0) {
			/* reset */
			SET_FLAG(adpt, ADPT_TASK_REINIT_REQ);
			emac_task_schedule(adpt);
			break;
		}
	} while (1);

	return IRQ_HANDLED;
}

static int emac_sgmii_v1_up(struct emac_adapter *adpt)
{
	struct emac_sgmii_v1 *sgmii = adpt->phy.private;
	int ret;

	ret = request_irq(sgmii->irq, emac_sgmii_v1_isr, IRQF_TRIGGER_RISING,
			  "sgmii_irq", adpt);
	if (ret)
		emac_err(adpt,
			 "error:%d on request_irq(%d:sgmii_irq)\n", ret,
			 sgmii->irq);

	/* enable sgmii irq */
	writel_relaxed(SGMII_ISR_MASK,
		       sgmii->base + EMAC_SGMII_PHY_INTERRUPT_MASK);

	return ret;
}

static void emac_sgmii_v1_down(struct emac_adapter *adpt)
{
	struct emac_sgmii_v1 *sgmii = adpt->phy.private;

	writel_relaxed(0, sgmii->base + EMAC_SGMII_PHY_INTERRUPT_MASK);
	synchronize_irq(sgmii->irq);
	free_irq(sgmii->irq, adpt);
}

static void emac_sgmii_v1_tx_clk_set_rate_nop(struct emac_adapter *adpt)
{
}

/* Check SGMII for error */
static void emac_sgmii_v1_periodic_check(struct emac_adapter *adpt)
{
	struct emac_sgmii_v1 *sgmii = adpt->phy.private;

	if (!TEST_FLAG(adpt, ADPT_TASK_CHK_SGMII_REQ))
		return;
	CLR_FLAG(adpt, ADPT_TASK_CHK_SGMII_REQ);

	/* ensure that no reset is in progress while link task is running */
	while (TEST_N_SET_FLAG(adpt, ADPT_STATE_RESETTING))
		msleep(20); /* Reset might take few 10s of ms */

	if (TEST_FLAG(adpt, ADPT_STATE_DOWN))
		goto sgmii_task_done;

	if (readl_relaxed(sgmii->base + EMAC_SGMII_PHY_RX_CHK_STATUS) & 0x40)
		goto sgmii_task_done;

	emac_err(adpt, "SGMII CDR not locked\n");

sgmii_task_done:
	CLR_FLAG(adpt, ADPT_STATE_RESETTING);
}

struct emac_phy_ops emac_sgmii_v1_ops = {
	.config			= emac_sgmii_v1_config,
	.up			= emac_sgmii_v1_up,
	.down			= emac_sgmii_v1_down,
	.init			= emac_sgmii_v1_init,
	.reset			= emac_sgmii_v1_reset,
	.init_ephy		= emac_sgmii_v1_init_ephy_nop,
	.link_setup_no_ephy	= emac_sgmii_v1_link_setup_no_ephy,
	.link_check_no_ephy	= emac_sgmii_v1_link_check_no_ephy,
	.tx_clk_set_rate	= emac_sgmii_v1_tx_clk_set_rate_nop,
	.periodic_task		= emac_sgmii_v1_periodic_check,
};
